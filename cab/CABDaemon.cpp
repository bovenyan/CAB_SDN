#include <arpa/inet.h>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <iostream>
#include <fstream>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <utility>
#include <vector>
#include "Message.hpp"
#include "TimeSpec.hpp"
#include "BucketTree.h"
#include "Rule.hpp"

using boost::asio::ip::tcp;

class Adapter {
public:
    Adapter(bucket_tree & bTree):bTree_(bTree),request_c(0) {
    }

    void process(Message & msg) {
        char * body = msg.body();
        addr_5tup pkt_h;
        for(uint32_t i = 0; i != 4; ++i) {
            uint32_t word = 0;
            memcpy(&word,body + i*4,4);
            word = ntohl(word);
            pkt_h.addrs[i] = word;
        }
        msg.clear();
        std::pair<bucket*, int> bpair;
        bpair = bTree_.search_bucket(pkt_h,bTree_.root);
        bucket * bkt = bpair.first;

        if( bkt != nullptr) {
            msg.append_uint(bkt->addrs[0].pref);
            msg.append_uint(bkt->addrs[0].mask);
            msg.append_uint(bkt->addrs[1].pref);
            msg.append_uint(bkt->addrs[1].mask);
            msg.append_uint(bkt->addrs[2].pref);
            msg.append_uint(bkt->addrs[2].mask);
            msg.append_uint(bkt->addrs[3].pref);
            msg.append_uint(bkt->addrs[3].mask);
            msg.append_uint(0);


            auto & rule_ids = bkt->related_rules;
            auto & rule_list = bTree_.rList->list;

            for (uint32_t id : rule_ids) {
                b_rule approx_bkt (rule_list[id].cast_to_bRule());
                // BOOST_LOG_TRIVIAL(debug) << rule_list[id].get_str() << endl;
                // BOOST_LOG_TRIVIAL(debug) << approx_b.get_str() << endl << endl;
                msg.append_uint(approx_bkt.addrs[0].pref);
                msg.append_uint(approx_bkt.addrs[0].mask);
                msg.append_uint(approx_bkt.addrs[1].pref);
                msg.append_uint(approx_bkt.addrs[1].mask);
                msg.append_uint(approx_bkt.addrs[2].pref);
                msg.append_uint(approx_bkt.addrs[2].mask);
                msg.append_uint(approx_bkt.addrs[3].pref);
                msg.append_uint(approx_bkt.addrs[3].mask);
                msg.append_uint(id);
                approx_bkt.print(); // jiaren20161116: Debug information for adding rules from controller to switch
            }

        }
        msg.encode_header();
        ++request_c;
    }
    unsigned long get_request_c() {
        return request_c.load();
    }

    void set_request_c(unsigned long i) {
        request_c = i;
    }

private:
    bucket_tree & bTree_;
    std::atomic_ulong request_c;

};

//----------------------------------------------------------------------
class session;
typedef std::shared_ptr<session> session_ptr;
typedef std::set<session_ptr> session_container;
//----------------------------------------------------------------------

class session
        : public std::enable_shared_from_this<session> {
public:
    session(tcp::socket && socket, session_container & container, Adapter & adapter)
        : socket_(std::forward<tcp::socket>(socket)),
          container_(container),
          adapter_(adapter) {
    }

    void start() {
        container_.insert(shared_from_this());
        do_read_header();
    }

    ~session() {
        if(socket_.is_open()) {
            socket_.close();
        }
    }

private:
    void do_read_header() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(msg_.data(), Message::header_length),
        [this, self](boost::system::error_code ec, std::uint32_t /*length*/) {
            if (!ec && msg_.decode_header()) {
                do_read_body();
            } else {
                handle_error(ec.message());
                container_.erase(shared_from_this());
            }
        });
    }

    void do_read_body() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(msg_.body(), msg_.body_length()),
        [this, self](boost::system::error_code ec, std::uint32_t /*length*/) {
            if (!ec) {
                //do our job.
                adapter_.process(msg_);
                do_write(msg_);
            } else {
                handle_error(ec.message());
                container_.erase(shared_from_this());
            }
        });
    }

    void do_write(const Message & msg) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(msg.data(),
                                         msg.length()),
        [this, self](boost::system::error_code ec, std::uint32_t /*length*/) {
            if (!ec) {
                do_read_header();
            } else {
                handle_error(ec.message());
            }
            container_.erase(shared_from_this());
        });
    }


    void handle_error(std::string message) {
        BOOST_LOG_TRIVIAL(error)  <<message << endl;
        if(socket_.is_open()) {
            socket_.close();
        }
    }

    tcp::socket socket_;
    session_container & container_;
    Adapter & adapter_;
    Message msg_;
};

//----------------------------------------------------------------------

class chat_server {
public:
    chat_server(boost::asio::io_service& ios,
                const tcp::endpoint& endpoint,
                Adapter & adapter)
        : ios_(ios),
          acceptor_(ios, endpoint),
          adapter_(adapter),
          skt(ios) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(skt,
        [this](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<session>(std::move(skt), container_, adapter_)->start();
            } else {
                BOOST_LOG_TRIVIAL(error) << ec.message() << endl;
            }

            do_accept();
        });
    }
    boost::asio::io_service & ios_;
    tcp::acceptor acceptor_;
    Adapter &adapter_;
    //session life cycle management
    std::set<std::shared_ptr<session>> container_;
    tcp::socket skt;
};

//----------------------------------------------------------------------
void boost_log_init() {
    namespace logging = boost::log;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;
//    logging::add_file_log
//    (
//        keywords::file_name = "sample_%N.log",
//        keywords::rotation_size = 10 * 1024 * 1024,
//        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
//        keywords::format = "[%TimeStamp%]: %Message%"
//    );
//
    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::debug
    );
}
void collector(Adapter & adp, std::ostream & os) {
    static TimeSpec zero(true);
    static TimeSpec to_sleep(1,0);
    while(true) {
        TimeSpec now;
        clock_gettime(CLOCK_REALTIME,&now.time_point_);
        os << now.time_point_.tv_sec<< "\t" << adp.get_request_c() << endl;
        // BOOST_LOG_TRIVIAL(debug) << now.time_point_.tv_sec<< "\t" << adp.get_request_c() << endl;
        adp.set_request_c(0);
        if(nanosleep(&to_sleep.time_point_,NULL)) {
            BOOST_LOG_TRIVIAL(warning) << "nanosleep failed." << endl;
        }
    }
}

void print_usage() {
    std::cerr << "Usage: CABDeamon <CABDaemon_config.ini> [rules_file] [port] [stat_file]\n"
              << "port          [default: 9000]\n"
              << "stat_file     [default: control.stat]"
              << std::endl;
}
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string rulesFileName;
    int ctrl_port;
    std::string statFileName;
    int iThresholdHard;
    bool bIs2tup;

    std::ofstream st_out;

    /* read initial configuration from .ini */
    std::ifstream iniFile(argv[1]);
    std::string strLine;
    std::vector<string> vector_parameters;

    for (; std::getline(iniFile, strLine); ) {
        vector_parameters = std::vector<string>();
        boost::split(vector_parameters, strLine, boost::is_any_of(" \t"));
        if ("rules_file" == vector_parameters[0]) {
            rulesFileName = vector_parameters[1];
        }
        if ("port" == vector_parameters[0]) {
            ctrl_port = std::stoi(vector_parameters[1]);
        }
        if ("stat_file" == vector_parameters[0]) {
            statFileName = vector_parameters[1];
        }
        if ("threshold_hard" == vector_parameters[0]) {
            iThresholdHard = std::stoi(vector_parameters[1]);
        }
        if ("2tup_or_4tup" == vector_parameters[0]) {
            if ("true" == vector_parameters[1]) bIs2tup = true;
            else bIs2tup = false;
        }
    }

    if (argc > 2) {
        rulesFileName = std::string(argv[2]);
    }
    if (argc > 3) {
        ctrl_port = atoi(argv[3]);
    }
    if (argc > 4) {
        statFileName = std::string(argv[4]);
    }

    st_out.open(statFileName);


    if (!st_out.is_open()) {
        std::cerr << "Can not open statistic file." << endl;
        return 2;
    }

    boost_log_init();

    /* parse rule file  */
    rule_list rList(rulesFileName, bIs2tup);

    BOOST_LOG_TRIVIAL(debug) << "Loading rules : "
                             << rList.list.size() << std::endl;

    /* Bucket Tree initialization and preloaing */
    bucket_tree bTree(rList, iThresholdHard, bIs2tup);
    bTree.pre_alloc();

    Adapter adapter(bTree);
    try {
        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), ctrl_port);

        /* start stats collector */
        std::thread collector_t(collector,std::ref(adapter),std::ref(st_out));
        collector_t.detach();
        BOOST_LOG_TRIVIAL(info) << "Collector running..." << endl;

        /* CABDaemon is the TCP server */
        chat_server server(io_service, endpoint , adapter);
        BOOST_LOG_TRIVIAL(info) << "Deamon running..." << endl;
        io_service.run();
    } catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(fatal) << "Exception: " << e.what() << "\n";
    }

    return 0;
}
