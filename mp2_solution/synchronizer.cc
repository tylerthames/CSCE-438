#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <thread>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include "sns.grpc.pb.h"
#include "snscoord.grpc.pb.h"
#include "synchro.grpc.pb.h"
using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using snsCoordinator::SNSCoordinator;
using snsCoordinator::CRequest;
using snsCoordinator::CReply;
using snsCoordinator::HeartBeat;
using synchronizer::Synchronizer;
using synchronizer::UserMessage;
using synchronizer::Response;

struct FileLog{
    std::string directory;
    std::string filename;
    int prevLength;
    std::string type;
};

struct Client{
    std::string username;
    std::vector<std::string> followers;
};


std::vector<Client> client_db;
std::vector<FileLog> file_db;
std::string f2port;
std::string f3port;
std::string sync_id;

std::unique_ptr<Synchronizer::Stub> s2stub_;
std::unique_ptr<Synchronizer::Stub> s3stub_;

void CreateUserFiles(std::string id){
    std::string mdir = "MASTER_" + id + "/";
    std::string sdir = "SLAVE_" + id + "/";
    std::string mfilename = "users.txt";
    std::string sfilename = "users.txt";
    std::string fname = mdir + mfilename;
    std::ofstream master(fname);
    master.close();
    fname = sdir + sfilename;
    std::ofstream slave(fname);
    slave.close();
    FileLog mfile;
    mfile.directory = mdir;
    mfile.type = "ONLINE";
    mfile.filename = mfilename;
    mfile.prevLength = 0;
    FileLog sfile;
    sfile.directory = sdir;
    sfile.type = "ONLINE";
    sfile.filename = sfilename;
    sfile.prevLength = 0;
    file_db.push_back(mfile);
    file_db.push_back(sfile);
}

void CreateUserFile(std::string user){
    std::string mdir = "MASTER_" + sync_id + "/";
    std::string sdir = "SLAVE_" + sync_id + "/";
    std::string mfilename = user + ".txt";
    std::string sfilename = user + ".txt";
    std::string fname = mdir + mfilename;
    std::ofstream master(fname);
    master.close();
    fname = sdir + sfilename;
    std::ofstream slave(fname);
    slave.close();
    FileLog mfile;
    mfile.directory = mdir;
    mfile.type = "USER";
    mfile.filename = mfilename;
    mfile.prevLength = 0;
    FileLog sfile;
    sfile.directory = sdir;
    sfile.type = "USER";
    sfile.filename = sfilename;
    sfile.prevLength = 0;
    file_db.push_back(mfile);
    file_db.push_back(sfile);
}

void CreateFollowerFile(std::string user){
    std::string mdir = "MASTER_" + sync_id + "/";
    std::string sdir = "SLAVE_" + sync_id + "/";
    std::string mfilename = user + "followers.txt";
    std::string sfilename = user + "followers.txt";
    std::string fname = mdir + mfilename;
    std::ofstream master(fname);
    master.close();
    fname = sdir + sfilename;
    std::ofstream slave(fname);
    slave.close();
    FileLog mfile;
    mfile.directory = mdir;
    mfile.type = "FOLLOWER";
    mfile.filename = mfilename;
    mfile.prevLength = 0;
    FileLog sfile;
    sfile.directory = sdir;
    sfile.type = "FOLLOWER";
    sfile.filename = sfilename;
    sfile.prevLength = 0;
    file_db.push_back(mfile);
    file_db.push_back(sfile);
}

void CreateFollowingFile(std::string user){
    std::string mdir = "MASTER_" + sync_id + "/";
    std::string sdir = "SLAVE_" + sync_id + "/";
    std::string mfilename = user + "following.txt";
    std::string sfilename = user + "following.txt";
    std::string fname = mdir + mfilename;
    std::ofstream master(fname);
    master.close();
    fname = sdir + sfilename;
    std::ofstream slave(fname);
    slave.close();
    FileLog mfile;
    mfile.directory = mdir;
    mfile.type = "FOLLOWING";
    mfile.filename = mfilename;
    mfile.prevLength = 0;
    FileLog sfile;
    sfile.directory = sdir;
    sfile.type = "FOLLOWING";
    sfile.filename = sfilename;
    sfile.prevLength = 0;
    file_db.push_back(mfile);
    file_db.push_back(sfile);
}

class SynchronizerImpl final : public Synchronizer::Service {
    
    Status UserUpdate(ServerContext* context, const UserMessage* request, Response* reply){
        if(request->id() != sync_id){
            std::string mdir = "MASTER_" + sync_id + "/";
            std::string sdir = "SLAVE_" + sync_id + "/";
            bool newUser = true;
            for(int i = 0; i < client_db.size(); i++){
                if(client_db[i].username == request->content()){
                    newUser = false;
                }
            }
            if(newUser == true){
                std::string mfile = mdir + request->filename();
                std::string sfile = sdir + request->filename();
                std::ofstream mstream(mfile,std::ios::app|std::ios::out|std::ios::in);
                mstream << request->content() << "\n";
                mstream.close();
                std::ofstream sstream(sfile,std::ios::app|std::ios::out|std::ios::in);
                sstream << request->content() << "\n";
                sstream.close();
                Client c;
                c.username = request->content();
                client_db.push_back(c);
                CreateUserFile(request->content());
                CreateFollowerFile(request->content());
                CreateFollowingFile(request->content());
            }
        }
        reply->set_msg("done");
        return Status::OK;
    }

    Status FollowerUpdate(ServerContext* context, const UserMessage* request, Response* reply){
        
        std::string mdir = "MASTER_" + sync_id + "/";
        std::string sdir = "SLAVE_" + sync_id + "/";
        bool newUser = true;
        Client* c;
        for(int i = 0; i < client_db.size(); i++){
            if(client_db[i].username == request->username()){
                c = &client_db[i];
            }
        }
        for(int i = 0; i < c->followers.size(); i++){
            if(c->followers[i] == request->content()){
                newUser= false;
            }
        }
        if(newUser == true){
            std::string mfile = mdir + request->filename();
            std::string sfile = sdir + request->filename();
            std::ofstream mstream(mfile,std::ios::app|std::ios::out|std::ios::in);
            mstream << request->content() << "\n";
            mstream.close();
            std::ofstream sstream(sfile,std::ios::app|std::ios::out|std::ios::in);
            sstream << request->content() << "\n";
            sstream.close();
            c->followers.push_back(request->content());
        }


        reply->set_msg("done");
        return Status::OK;
    }

    Status TimelineUpdate(ServerContext* context, const UserMessage* request, Response* reply){
        std::string mdir = "MASTER_" + sync_id + "/";
        std::string sdir = "SLAVE_" + sync_id + "/";
        std::string mfile = mdir + request->filename();
        std::string sfile = sdir + request->filename();
        std::ofstream mstream(mfile,std::ios::app);
        mstream << request->content();
        mstream.close();
        std::ofstream sstream(sfile,std::ios::app);
        sstream << request->content();
        sstream.close();
        return Status::OK;
    }
};

void ConnectSynchros(){
    std::string s2login = "0.0.0.0:" + f2port;
    std::string s3login = "0.0.0.0:" + f3port;
    s2stub_ = std::unique_ptr<Synchronizer::Stub>(Synchronizer::NewStub(
               grpc::CreateChannel(
                    s2login, grpc::InsecureChannelCredentials())));
    s3stub_ = std::unique_ptr<Synchronizer::Stub>(Synchronizer::NewStub(
               grpc::CreateChannel(
                    s3login, grpc::InsecureChannelCredentials())));
    
}

void CLogin(std::string port_no, std::string cport, std::string cip,  int id){
    std::string login_info = cip + ":" + cport;

    //Creating the coordinator request
    CRequest req;
    req.set_port_number(port_no);
    req.set_server_type("SYNCHRONIZER");
    req.set_id(id);
    req.set_requester("SERVER");
    CReply rep;

    //Connecting to the coordinator
    ClientContext context;
    std::unique_ptr<SNSCoordinator::Stub> stub_;
    stub_ = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
    Status status = stub_->CLogin(&context, req, &rep);
}



int FileLines(std::string filename){
    std::ifstream stream(filename);
    int i = 0;
    std::string text;
    while(stream.peek()!=EOF){
        std::getline(stream, text);
        i++;
    }
    return i;
}

void UserUpdates(){
    FileLog* users;
    std::string dir = "SLAVE_" + sync_id + "/";
    for(int i = 0; i < file_db.size(); i++){
        if(file_db.at(i).directory == dir && file_db.at(i).filename == "users.txt"){
            users = &file_db[i];
            break;
        }
    }
    std::string ufile = users->directory + users->filename;
    int update_num = FileLines(ufile);
    if(update_num > users->prevLength){
        
        std::ifstream readUsers(ufile);
        Client newclient;
        std::string text;
        for(int i = 0; i < users->prevLength; i++){
            std::getline(readUsers,text);
        }
        bool isNew;
        while(std::getline(readUsers,text)){
            isNew = true;
            for(int i = 0; i < client_db.size(); i++){
                if(client_db[i].username == text){
                    isNew = false;
                }
            }
            if(isNew){
                newclient.username = text;
                CreateUserFile(text);
                CreateFollowerFile(text);
                CreateFollowingFile(text);
                client_db.push_back(newclient);
                ClientContext context2;
                ClientContext context3;
                UserMessage msg;
                Response rep;
                msg.set_filename("users.txt");
                msg.set_content(text);
                msg.set_id(sync_id);
                Status status2 = s2stub_->UserUpdate(&context2, msg, &rep);
                Status status3 = s3stub_->UserUpdate(&context3, msg, &rep);
            }
            
        }
        users->prevLength = update_num;
    }
}

void FollowerUpdates(){
    FileLog* users;
    Client* c;
    std::string dir = "SLAVE_" + sync_id + "/";
    std::vector<FileLog*> followerfiles;
    for(int i = 0; i < file_db.size(); i++){
        if(file_db.at(i).directory == dir && file_db.at(i).type == "FOLLOWER"){
            users = &file_db[i];

            std::string ufile = users->directory + users->filename;
            int update_num = FileLines(ufile);
            std::string uname1 = users->filename.substr(0,2);
            for(int j = 0; j < client_db.size(); j++){
                if(uname1 == client_db[j].username){
                    c = &client_db[j];
                }
            }
            if(update_num > users->prevLength){
                std::ifstream readUsers(ufile);
                std::string text;
                for(int i = 0; i < users->prevLength; i++){
                    std::getline(readUsers,text);
                }
                bool isNew;
                while(std::getline(readUsers,text)){
                    isNew = true;
                    std::string uname2 = text.substr(0,2);
                    for(int j = 0; j < c->followers.size(); j++){
                        if(uname2 == c->followers[j]){
                            isNew = false;
                        }
                    }
                    if(isNew && uname2.at(0) == 'u'){
                        c->followers.push_back(uname2);
                        ClientContext context2;
                        ClientContext context3;
                        UserMessage msg;
                        Response rep;
                        msg.set_filename(users->filename);
                        msg.set_content(uname2);
                        msg.set_username(uname1);
                        msg.set_id(sync_id);
                        Status status2 = s2stub_->FollowerUpdate(&context2, msg, &rep);
                        Status status3 = s3stub_->FollowerUpdate(&context3, msg, &rep);
                    }
                }
                users->prevLength = update_num;
            }
        }
    }

}

void AddTimelinePosts(std::vector<std::string> lines, std::string user){
    //Find the master and slave following files for the given user
    for(int i = 0; i < file_db.size(); i++){
        std::string username = file_db[i].filename.substr(0,2);
        if(user == username && file_db[i].type == "FOLLOWING"){
            std::string fname = file_db[i].directory + file_db[i].filename;
            std::ofstream outfile(fname,std::ios::app|std::ios::out|std::ios::in);
            for(int j = 0; j < lines.size(); j++){
                outfile << lines[j] << "\n";
                file_db[i].prevLength++;

                //Send to other synchronizers
                ClientContext context2;
                ClientContext context3;
                UserMessage msg;
                Response rep;
                msg.set_username(username);
                msg.set_filename(file_db[i].filename);
                msg.set_content(lines[j]+"\n");
                msg.set_id(sync_id);
                Status status2 = s2stub_->TimelineUpdate(&context2, msg, &rep);
                Status status3 = s3stub_->TimelineUpdate(&context3, msg, &rep);
            }
        }
    }
}

void TimelineUpdates(){
//Loop through every "USER" file in the slave db
std::string slavedir = "SLAVE_" + sync_id + "/";
std::string masterdir = "MASTER_" + sync_id + "/";
for(int i = 0; i < file_db.size(); i++){
    if(file_db[i].type == "USER" && file_db[i].directory == slavedir){
        //Check if the file has any new lines
        std::string ufilename = file_db[i].directory + file_db[i].filename;
        int totalLines = FileLines(ufilename);
        int newlines = totalLines - file_db[i].prevLength;
        if(newlines == 0){
            continue;
        }
        else{
            //Fill a vector with all new lines from the file.
            std::vector<std::string> lines;
            std::ifstream userstream(ufilename);
            std::string text;
            for(int j = 0; j < file_db[i].prevLength; j++){
                std::getline(userstream,text);
            }
            while(std::getline(userstream,text)){
                lines.push_back(text);
            }
            userstream.close();
            file_db[i].prevLength = totalLines;

            //Find the client matching that file and update the following files for all other users
            std::string user = file_db[i].filename.substr(0,2);
            for(int j = 0; j < client_db.size(); j++){
                if(user == client_db[j].username){
                    AddTimelinePosts(lines, user);
                    for(int k = 0; k < client_db[j].followers.size(); k++){
                        AddTimelinePosts(lines, client_db[j].followers[k]);
                    }
                }
            }
        }
    }
}


}

void SetPorts(std::string port){
    if(port == "9790"){
        f2port = "9990";
        f3port = "9890";
    }
    else if(port == "9890"){
        f2port = "9790";
        f3port = "9990";
    }
    else{
        f2port = "9890";
        f3port = "9790";
    }
}

void RunServer(std::string port){
    std::string server_address = "0.0.0.0:"+port;
    SynchronizerImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Synchronizer listening on " << server_address << std::endl;

    server->Wait();
}

void RunSynchronizer(std::string id, std::string port){
    sleep(1);
    CreateUserFiles(id);
    std::thread run_server(RunServer, port);
    ConnectSynchros();
    while(true){
        UserUpdates();
        FollowerUpdates();
        TimelineUpdates();
        sleep(30);
    }
}

int main(int argc, char** argv) {

    std::string port = "3010";
    std::string cip = "0.0.0.0";
    std::string cport = "9090";
    std::string id;
    int opt = 0;
    while ((opt = getopt(argc, argv, "c:h:p:i:")) != -1){
        switch(opt) {
        case 'p':
            port = optarg;break;
        case 'h':
            cip = optarg;break;
        case 'c':
            cport = optarg;break;
        case 'i':
            id = optarg;break;
        default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }
    sync_id = id;
    int id_no = std::stoi(id);
    SetPorts(port);
    CLogin(port, cport, cip, id_no);
    RunSynchronizer(id, port);
    return 0;
}