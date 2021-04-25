#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <thread>
//namespace alias
namespace beast=boost::beast;
namespace http=beast::http;
namespace net=boost::asio;

//typename alias
using tcp=net::ip::tcp;

template <typename Body,typename Allocator,typename Send>
void handle_request(
    std::string doc_root,
    http::request<Body,http::basic_fields<Allocator>> &&req,
    Send &&send)
{
    //get index.html file
    std::string path=doc_root;
    boost::string_view target=req.target();
    path+=target.to_string();
    if(path.back()=='/')
        path+="index.html";

    //open file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(),beast::file_mode::scan,ec);


    auto const size=body.size();
    //Respond to Get request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok,req.version())};
    res.set(http::field::server,BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type,"text/html");
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}
void
fail(beast::error_code ec, char const *what)
{
    std::cerr << what <<": " << ec.message() << "\n";
}
class Session : public std::enable_shared_from_this<Session>
{
    struct send_lambda
    {
        Session &m_self;
        explicit send_lambda(Session &self)
            :m_self(self){}

        template<bool isRequest,typename Body,typename Fields>
        void operator()(http::message<isRequest, Body, Fields>&&msg) const
        {
            auto sp=std::make_shared<
                http::message<isRequest,Body,Fields>>(std::move(msg));
            m_self.m_res=sp;

            //write the response
            http::async_write(
                m_self.m_stream,
                *sp,
                beast::bind_front_handler(
                    &Session::on_write,
                    m_self.shared_from_this(),
                    sp->need_eof()));
        }
    };
    beast::tcp_stream m_stream;
    beast::flat_buffer m_buffer;
    std::shared_ptr<std::string const> m_doc_root;
    http::request<http::string_body> m_req;
    std::shared_ptr<void>m_res;
    send_lambda m_lambda;
public:
        Session(
            tcp::socket &&socket,
            std::shared_ptr<std::string const> const &doc_root)
            :m_stream(std::move(socket)),
             m_doc_root(doc_root),
             m_lambda(*this){}

        void run()
        {
            net::dispatch(m_stream.get_executor(),
                          beast::bind_front_handler(
                              &Session::do_read,
                              shared_from_this()));
        }

        void do_read()
        {
            m_req={};

            m_stream.expires_after(std::chrono::seconds(30));

            http::async_read(m_stream,
                             m_buffer,
                             m_req,
                             beast::bind_front_handler(
                                 &Session::on_read, shared_from_this()));

        }

        void on_read(beast::error_code ec,
                     std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if(ec==http::error::end_of_stream)
                return do_close();
            if(ec)
                return fail(ec,"write");

            handle_request(*m_doc_root,std::move(m_req),m_lambda);
        }

        void on_write(
            bool close,
            beast::error_code ec,
            std::size_t bytes_transferred
        )
        {
            boost::ignore_unused(bytes_transferred);

            if(ec)
                return fail(ec,"write");
            if(close)
                return do_close();
            m_res=nullptr;

            do_read();
        }

        void do_close()
        {
            beast::error_code ec;

            m_stream.socket().shutdown(tcp::socket::shutdown_send,ec);

        }


};

class Listener : public std::enable_shared_from_this<Listener>
{
    private:
        net::io_context &m_ioc;
        tcp::acceptor m_acceptor;
        std::shared_ptr<std::string const> m_doc_root;
    public:
        Listener(net::io_context& ioc,
                 tcp::endpoint endpoint,
                 std::shared_ptr<std::string const> const& doc_root)
            :m_ioc(ioc),
             m_acceptor(net::make_strand(ioc)),
             m_doc_root(doc_root)
        {
            beast::error_code ec;

            m_acceptor.open(endpoint.protocol(),ec);

            if(ec)
            {
                fail(ec,"open");
                return;
            }

            m_acceptor.set_option(net::socket_base::reuse_address(true),ec);

            if(ec)
            {
                fail(ec,"set_option");
                return;
            }

            m_acceptor.bind(endpoint,ec);

            if(ec)
            {
                fail(ec,"bind");
                return;
            }

            m_acceptor.listen(net::socket_base::max_listen_connections,ec);
            if(ec)
            {
                fail(ec,"lister");
                return;
            }
        }

        void run()
        {
            do_accept();
        }
    private:
        void do_accept()
        {
            m_acceptor.async_accept(net::make_strand(m_ioc),
                                    beast::bind_front_handler(
                                        &Listener::on_accept,
                                        shared_from_this()));

        }
        void on_accept(beast::error_code ec,tcp::socket socket)
        {
            if(ec)
            {
                fail(ec,"accept");
            }
            else
            {
                std::make_shared<Session>(
                    std::move(socket),
                    m_doc_root
                    )->run();
            }
            do_accept();
        }
};
class A
{
    int bsd;
};

int  main(int argc, char *argv[])
{

    if(argc!=5)
    {
        std::cerr << "out <address> <port> <root> <threads>\n";
        return EXIT_FAILURE;
    }
    std::cerr <<argv[1]<<' ' <<argv[2] << ' '<< argv[3] << argv[4] <<'\n';
    auto const address=net::ip::make_address(argv[1]);
    auto const port=static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root=std::make_shared<std::string>(argv[3]);
    auto const threads=std::max<int>(1,std::atoi(argv[4]));

    net::io_context ioc{threads};

    std::make_shared<Listener>(
        ioc,
        tcp::endpoint{address,port},
        doc_root)->run();

    std::vector<std::thread> v;
    v.reserve(threads-1);
    for(auto i=threads-1;i>0;--i)
        v.emplace_back([&ioc]{ioc.run();});
    ioc.run();

    return EXIT_SUCCESS;
    return 0;
}
