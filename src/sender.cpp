#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <boost/asio.hpp>
#include <boost/archive/iterators/istream_iterator.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using boost::asio::ip::tcp;

//******************************************************************************
//******************************************************************************
void makeTextField(const std::string & name, const std::string & value, std::ostringstream & oss)
{
    oss << "Content-Disposition: form-data; name=\"" << name << "\"\r\n";
    oss << "Content-Type: text/plain\r\n";
    oss << "Content-Transfer-Encoding: quoted-printable\r\n\r\n";
    oss << value << "\r\n";

}

//******************************************************************************
//******************************************************************************
void makeFileField(const std::string & name, const std::string & filename,
                   const std::string & filepath, std::ostringstream & oss)
{
    oss << "Content-Disposition: form-data; name=\"" << name << "\"; filename=\"" << filename << "\"\r\n";
    oss << "Content-Type: application/octet-stream\r\n\r\n";
    //oss << "Content-Transfer-Encoding: binary\r\n\r\n";
    std::ifstream ifs(filepath.c_str(), std::ios_base::binary);
    //oss << ifs.rdbuf() << "\r\n";
    while (ifs.good())
    {
        oss << std::setfill('0') << std::setw(2) << std::hex << (unsigned short)ifs.get();
    }
    oss << "\r\n";
    ifs.close();
    return;
}

//******************************************************************************
//******************************************************************************
void makeFile(const std::string & filepath, std::ostringstream & oss)
{
    std::ifstream ifs(filepath.c_str(), std::ios_base::binary);
    while (ifs.good())
    {
        oss << std::setfill('0') << std::setw(2) << std::hex << (unsigned short)ifs.get();
    }
    ifs.close();
    return;
}

//******************************************************************************
//******************************************************************************
int sendBugReport(const std::string & fileName, const std::string & name,
                  const std::string & version, const std::string & mailto)
{
    const std::string URI("/BugReports/MessageReceiver.cgi");
    const std::string HOST("aktivsystems.ru");
    const std::string PORT("80");

    try
    {
        boost::asio::io_service io_service;

        // Get a list of endpoints corresponding to the server name.
        tcp::resolver resolver(io_service);
        tcp::resolver::query query(HOST, PORT);
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

        // Try each endpoint until we successfully establish a connection.
        tcp::socket socket(io_service);
        boost::asio::connect(socket, endpoint_iterator);

        std::string boundary = "AUZ_multipart_boundary";
        std::ostringstream request_data;

        request_data << "--" << boundary << "\r\n";
        makeTextField("MailFrom", "MailSender@aktivsystems.ru", request_data);

        std::ostringstream body;
        body << "Name: " << name << "\r\n";
        body << "Version: " << version << "\r\n";

        request_data << "--" << boundary << "\r\n";
        makeTextField("MailBody", body.str(), request_data);

        request_data << "--" << boundary << "\r\n";
        makeTextField("MailSubject", std::string("Bug Report from ") + name, request_data);
        request_data << "--" << boundary << "\r\n";
        //makeTextField("MailTo", "arackcheev@aktivsystems.ru", request_data);

        makeTextField("MailTo", mailto, request_data);

        request_data << "--" << boundary << "\r\n";
        makeFileField("File", "minidump.dmp", fileName, request_data);

//        if (argc > 5)
//        {
//            request_data << "--" << boundary << "\r\n";
//            makeFileField("File", "screenshot.bmp", argv[5], request_data);
//        }

        request_data << "--" << boundary << "--\r\n";

        //std::cout << request_data.str();
        //return 0;

        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        boost::asio::streambuf request;
        std::ostream request_stream(&request);
        request_stream << "POST " << URI << " HTTP/1.0\r\n";
        request_stream << "Host: " << HOST << "\r\n";
        request_stream << "Content-Type: application/json\r\n";
        request_stream << "Content-Length: " << request_data.str().size() << "\r\n\r\n";

        request_stream << request_data.str() << "\r\n";

        //std::cout << request_stream.str() << std::endl;
        //return 0;

        // Send the request.
        boost::asio::write(socket, request);

        // Read the response status line. The response streambuf will automatically
        // grow to accommodate the entire line. The growth may be limited by passing
        // a maximum size to the streambuf constructor.
        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\r\n");

        // Check that response is OK.
        std::istream response_stream(&response);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            std::cout << "Invalid response\n";
            return 1;
        }
        if (status_code != 200)
        {
            std::cout << "Response returned with status code " << status_code << "\n";
            return 2;
        }

        // Read the response headers, which are terminated by a blank line.
        boost::asio::read_until(socket, response, "\r\n\r\n");

        // Process the response headers.
        std::string header;
        while (std::getline(response_stream, header) && header != "\r")
        {
            std::cout << header << "\n";
        }
        std::cout << "\n";

        // Write whatever content we already have to output.
        if (response.size() > 0)
        {
            std::cout << &response;
        }

        // Read until EOF, writing data to output as we go.
        boost::system::error_code error;
        while (boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error))
        {
            std::cout << &response;
        }

        if (error != boost::asio::error::eof)
        {
            throw boost::system::system_error(error);
        }
    }

    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }

    return 0;
}

