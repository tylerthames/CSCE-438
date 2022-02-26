#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------

    std::string hostport = hostname + ":" + port;
    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(hostport, grpc::InsecureChannelCredentials())));

    
    ClientContext clientcontext;
    Status status;
    Request req;
    Reply rep;
    
    req.set_username(username);
    status = stub_->Login(&clientcontext, req, &rep);
    if(rep.msg() == "Error: user already online"){
        return -1;
    }

    

    return 1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    
    IReply ire;

    ClientContext clientcontext;
    Status status;
    Request req;
    Reply rep;

    std::stringstream sstream(input);
    std::string instring;
    std::string user;
    sstream >> instring;
    

    if(instring == "FOLLOW"){
        sstream >> user;

        req.set_username(username);
        req.add_arguments(user);
        status = stub_->Follow(&clientcontext, req, &rep);
        ire.grpc_status = status;
        //Handle reply messages once server code is done
        if(rep.msg() == "Error: User does not exist" || rep.msg() == "Error: Cannot follow self"){
            ire.comm_status = FAILURE_INVALID_USERNAME;
        }
        else if(rep.msg() == "Error: already following"){
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        }
        else if(rep.msg() == "Successfully followed"){
            ire.comm_status = SUCCESS;
        }
        else{
            ire.comm_status = FAILURE_UNKNOWN;
        }

    }
    else if(instring == "UNFOLLOW"){
        sstream >> user;
        req.set_username(username);
        req.add_arguments(user);
        status = stub_->UnFollow(&clientcontext, req, &rep);
        ire.grpc_status = status;

        if(rep.msg() == "Error: User does not exist" || rep.msg() == "Error: Cannot unfollow self"){
            ire.comm_status = FAILURE_INVALID_USERNAME;
        }
        else if(rep.msg() == "Error: not following"){
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
        else if(rep.msg() == "Successfully unfollowed"){
            ire.comm_status = SUCCESS;
        }
        else{
            ire.comm_status = FAILURE_UNKNOWN;
        }

    }
    
    else if(instring == "LIST"){
        req.set_username(username);
        status = stub_->List(&clientcontext, req, &rep);
        ire.grpc_status = status;
        if(status.ok()){
            ire.comm_status = SUCCESS;
            for(std::string user : rep.all_users()){
                ire.all_users.push_back(user);
            }
            for(std::string user : rep.following_users()){
                ire.following_users.push_back(user);
            }
            
        }
        else{
            ire.comm_status = FAILURE_UNKNOWN;
        }
    }
    else if(instring == "TIMELINE"){
        ire.comm_status = SUCCESS;
    }
    else{
        ire.comm_status = FAILURE_INVALID;
    }


    return ire;
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------
    ClientContext clientcontext;
    std::string name = username;
    //Create the message stream between client and server
    std::shared_ptr<ClientReaderWriter<Message,Message>> stream(stub_->Timeline(&clientcontext));

    //Two threads needed, one to write messages to server and one to read messages from server
    std::thread writingThread([name, stream](){
        std::string getMsg;
        Message msg;
        msg.set_username(name);
        //Send timeline dump to get the most recent 20 messages
        msg.set_msg("Timeline dump");
        stream->Write(msg);
        while(true){
            getMsg = getPostMessage();
            msg.set_msg(getMsg);
            stream->Write(msg);
        }
        stream->WritesDone();
    });


    std::thread readingThread([name, stream](){
        Message msg;
        
        while(stream->Read(&msg)){
            std::time_t timestamp = msg.timestamp().seconds();
            displayPostMessage(msg.username(), msg.msg(), timestamp);
        }
    });

    //join the two threads
    writingThread.join();
    readingThread.join(); 
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
}
