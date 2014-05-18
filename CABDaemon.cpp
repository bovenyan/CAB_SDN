#include <arpa/inet.h>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <boost/asio.hpp>
#include <utility>
#include "Message.hpp"
#include "BucketTree.h"

using boost::asio::ip::tcp;


class Adapter {
  public:
    Adapter(bucket_tree && bTree):bTree_(std::forward<bucket_tree>(bTree)) {
    }

    void process(Message & msg) {
        char * body = msg.body();
        addr_5tup pkt_h;
        for(uint32_t i = 0; i != 4; ++i) {
            uint32_t word = 0;
            memcpy(&word,body + i*4,4);
            word = ntohl(word);
            pkt_h.addrs[i] = word;
            std::cerr << pkt_h.addrs[i] << std::endl;
        }
        msg.clear();
        bucket * b = nullptr;

        b = bTree_.search_bucket(pkt_h,bTree_.root);
        if(b != nullptr) {
            msg.append_uint(b->addrs[0].pref);
            msg.append_uint(b->addrs[0].mask);
            msg.append_uint(b->addrs[1].pref);
            msg.append_uint(b->addrs[1].mask);
            msg.append_uint(b->addrs[2].pref);
            msg.append_uint(b->addrs[2].mask);
            msg.append_uint(b->addrs[3].pref);
            msg.append_uint(b->addrs[3].mask);

            auto & rule_ids = b->related_rules;
            std::cerr <<"rules number : " <<b->related_rules.size() << std::endl;
            auto & rule_list = bTree_.rList->list;
            for(uint32_t id : rule_ids) {
                msg.append_uint(rule_list[id].hostpair[0].pref);
                msg.append_uint(rule_list[id].hostpair[0].mask);
                msg.append_uint(rule_list[id].hostpair[1].pref);
                msg.append_uint(rule_list[id].hostpair[1].mask);
                msg.append_uint(rule_list[id].portpair[0].range[0]);
                msg.append_uint(rule_list[id].portpair[0].range[1]);
                msg.append_uint(rule_list[id].portpair[1].range[0]);
                msg.append_uint(rule_list[id].portpair[1].range[1]);
            }

        }
        std::cerr <<"reply message length : "<< msg.length() << std::endl;
        msg.encode_header();
    }
  private:
    bucket_tree bTree_;
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
                handle_error();
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
                socket_.shutdown(tcp::socket::shutdown_receive);
                //do our job.
                adapter_.process(msg_);
                do_write(msg_);
            } else {
                handle_error();
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
                socket_.shutdown(tcp::socket::shutdown_send);
            } else {
                handle_error();
            }
            container_.erase(shared_from_this());
        });
    }


    void handle_error() {
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
                Adapter && adapter)
        : ios_(ios),
          acceptor_(ios, endpoint),
          adapter_(std::forward<Adapter>(adapter)) {
        do_accept();
    }

  private:
    void do_accept() {
        tcp::socket skt(ios_);
        acceptor_.async_accept(skt,
        [&](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<session>(std::move(skt), container_, adapter_)->start();
            }

            do_accept();
        });
    }
    boost::asio::io_service & ios_;
    tcp::acceptor acceptor_;
    Adapter adapter_;
    //session life cycle management
    std::set<std::shared_ptr<session>> container_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if(argc < 3) {
        std::cerr << "Usage: CABDeamon {/path/to/rule/file} <port>"
                  << std::endl;
    }
    std::cerr << sizeof(uint32_t) << std::endl;
    //initialize CAB
    std::string rulefile(argv[1]);
    rule_list rList(rulefile);
    std::cerr << "loading rule : " << rList.list.size() << std::endl;
    bucket_tree bTree(rList, uint32_t(20));
    Adapter adapter(std::move(bTree));

    try {
        boost::asio::io_service io_service;

        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[2]));
        chat_server server(io_service, endpoint , std::move(adapter));

        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
