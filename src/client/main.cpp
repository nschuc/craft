#include <iostream>
#include <array>
#include <asio.hpp>
#include <thread>
#include "../shared/chat_message.hpp"

using namespace std;
using asio::ip::tcp;


class chat_client {
public:
    chat_client(asio::io_service& io_service,
        tcp::resolver::iterator endpoint_iterator)
            : io_service_(io_service),
              socket_(io_service)
    {
        do_connect(endpoint_iterator);
    }

    void write(const chat_message& msg) {
        io_service_.post([this, msg](){
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            if(!write_in_progress) {
                do_write();
            }
        });
    }

    void close(){
        io_service_.post([this](){ socket_.close(); });
    }
private:
    void do_connect(tcp::resolver::iterator endpoint_iterator) {
        asio::async_connect(socket_, endpoint_iterator,
        [this](asio::error_code ec, tcp::resolver::iterator){
            if(!ec){
                do_read_header();
            }
            else {
                cout << ec.message() << endl;
                close();
            }
        });
    }
    void do_read_header() {
        asio::async_read(socket_,
            asio::buffer(read_msg_.data(), chat_message::header_length),
            [this](error_code ec, std::size_t /*length*/) {
                if(!ec && read_msg_.decode_header()){
                    do_read_body();
                }
                else {
                    socket_.close();
                }
            }
        );
    }

    void do_read_body() {
        asio::async_read(socket_,
            asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](std::error_code ec, std::size_t /*length*/){
                if(!ec){
                    std::cout.write(read_msg_.body(), read_msg_.body_length());
                    std::cout << "\n";
                    do_read_header();
                }
                else
                {
                    socket_.close();
                }
            }
        );
    }

    void do_write() {
        asio::async_write(socket_,
         asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
         [this](std::error_code ec, std::size_t /*length*/){
             if(!ec){
                 write_msgs_.pop_front();
                 if(!write_msgs_.empty()){
                     //do_write();
                 }
             }
             else
             {
                 socket_.close();
             }
         }
        );
    }
    asio::io_service &io_service_;
    tcp::socket socket_;
    chat_message read_msg_;
    chat_message_queue write_msgs_;
};

int main(int argc, char* argv[]) {
    try {
        if(argc != 3) {
            std::cerr < "Usage: client <host <port>\n";
            return 1;
        }

        asio::io_service io_service;
        tcp::resolver resolver(io_service);
        auto endpoint_iter = resolver.resolve({argv[1], argv[2]});
        chat_client c(io_service, endpoint_iter);

        std::thread t([&io_service]{
            io_service.run();
        });

        char line[chat_message::max_body_length + 1];
        while(std::cin.getline(line, chat_message::max_body_length + 1)) {
            chat_message msg;
            msg.body_length(std::strlen(line));
            std::memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            c.write(msg);
        }
        c.close();
        t.join();
    }
    catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
    }


    return 0;
}