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
#include "Message.hpp"
#include "BucketTree.h"

using boost::asio::ip::tcp;


class Adapter
{
public:
    Adapter(bucket_tree & bTree):bTree_(bTree)
    {
    }

    void process(Message & msg)
    {
        char * body = msg.body();
        addr_5tup pkt_h;
        for(size_t i = 0; i != 4; ++i)
        {
            uint32_t word = 0;
            memcpy(&word,body + i*4,4);
            word = ntohl(word);
            pkt_h.addrs[i] = word;
            std::cerr << pkt_h.addrs[i] << std::endl;
        }
        msg.clear();
        bucket * b = nullptr;

        b = bTree_.search_bucket(pkt_h,bTree_.root);
        if(b != nullptr)
        {
            auto & rule_ids = b->related_rules;
            std::cerr <<"rules number : " <<b->related_rules.size() << std::endl;
            auto & rule_list = bTree_.rList->list;
            for(size_t id : rule_ids)
            {
                msg.append_uint(rule_list[id].hostpair[0].pref);
                msg.append_uint(rule_list[id].hostpair[0].mask);
                msg.append_uint(rule_list[id].hostpair[1].pref);
                msg.append_uint(rule_list[id].hostpair[1].mask);
                std::cerr << "body_length_ : " << msg.body_length() << std::endl;
            }

        }
        std::cerr <<"reply message length : "<< msg.length() << std::endl;
        msg.encode_header();
    }
private:
    bucket_tree & bTree_;
};





//----------------------------------------------------------------------
class session;
typedef std::shared_ptr<session> session_ptr;
typedef std::set<session_ptr> session_container;
//----------------------------------------------------------------------

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket, session_container & container, Adapter & adapter)
        : socket_(std::move(socket)),
          container_(container),
          adapter_(adapter)
    {
    }

    void start()
    {
        container_.insert(shared_from_this());
        do_read_header();
    }

    ~session()
    {
        if(socket_.is_open())
        {
            socket_.close();
        }
    }
private:
    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(msg_.data(), Message::header_length),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec && msg_.decode_header())
            {
                do_read_body();
            }
            else
            {
                handle_error();
                container_.erase(shared_from_this());
            }
        });
    }

    void do_read_body()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(msg_.body(), msg_.body_length()),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                socket_.shutdown(tcp::socket::shutdown_receive);
                //do our job.
                adapter_.process(msg_);
                do_write(msg_);
            }
            else
            {
                handle_error();
                container_.erase(shared_from_this());
            }
        });
    }

    void do_write(const Message & msg)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(msg.data(),
                                         msg.length()),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                socket_.shutdown(tcp::socket::shutdown_send);
            }
            else
            {
                handle_error();
            }
            container_.erase(shared_from_this());
        });
    }


    void handle_error()
    {
        if(socket_.is_open())
        {
            socket_.close();
        }
    }

    tcp::socket socket_;
    session_container & container_;
    Adapter & adapter_;
    Message msg_;
};

//----------------------------------------------------------------------

class chat_server
{
public:
    chat_server(boost::asio::io_service& io_service,
                const tcp::endpoint& endpoint,
                Adapter & adapter)
        : acceptor_(io_service, endpoint),
          socket_(io_service),
          adapter_(adapter)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(socket_,
                               [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                std::make_shared<session>(std::move(socket_), container_, adapter_)->start();
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    //session life cycle management
    std::set<std::shared_ptr<session>> container_;
    Adapter & adapter_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        std::cerr << "Usage: CABDeamon {/path/to/rule/file} <port>"
                  << std::endl;
    }

    //initialize CAB
    std::string rulefile(argv[1]);
    rule_list rList(rulefile);
    std::cerr << "loading rule : " << rList.list.size() << std::endl;
    bucket_tree bTree(rList, size_t(20));

    Adapter adapter(bTree);

    try
    {
        boost::asio::io_service io_service;

        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[2]));
        chat_server server(io_service, endpoint , adapter);

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}








