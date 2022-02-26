#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;


struct User{
    std::string username;
    //Lists of everyone the users follows and their followers
    std::vector<std::string> following;
    std::vector<std::string> followers;
    bool online;
    //User's personal message stream
    ServerReaderWriter<Message, Message>* stream = 0;
};

std::vector<User> users;

int get_user_index(std::string username){
    for(int i = 0; i < users.size(); i++){
        if(users.at(i).username == username){
          return i;
        }
    }
    return -1;
}

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    std::string username1 = request->username();
    int userindex1 = get_user_index(username1);
    

    for(int i = 0; i< users.size(); i++){
      reply->add_all_users(users.at(i).username);
    }
    

    for(int i = 0; i < users.at(userindex1).followers.size(); i++){
      reply->add_following_users(users.at(userindex1).followers.at(i));
    }


    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    std::string username1 = request->username();
    int userindex1 = get_user_index(username1);
    std::string username2 = request->arguments(0);
    int userindex2 = get_user_index(username2);
    std::string msg;

    if(userindex2 == -1){
        msg = "Error: User does not exist";
    }
    else if(userindex1 == userindex2){
        msg = "Error: Cannot follow self";
    }
    else{
        User *user1 = &users.at(userindex1);
        User *user2 = &users.at(userindex2);
        bool following = false;
        for(int i = 0; i < user1->following.size(); i++){
          if(user1->following.at(i) == username2){
            following = true;
          }
        }

        if(following){
          msg = "Error: already following";
        }
        else{
          msg = "Successfully followed";
          user1->following.push_back(username2);
          user2->followers.push_back(username1);
        }
    }

    reply->set_msg(msg);

    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    std::string username1 = request->username();
    int userindex1 = get_user_index(username1);
    std::string username2 = request->arguments(0);
    int userindex2 = get_user_index(username2);
    std::string msg;

    if(userindex2 == -1){
        msg = "Error: User does not exist";
    }
    else if(userindex1 == userindex2){
        msg = "Error: Cannot unfollow self";
    }
    else{
        User *user1 = &users.at(userindex1);
        User *user2 = &users.at(userindex2);
        bool following = false;
        int followingindex;
        int followerindex;
        for(int i = 0; i < user1->following.size(); i++){
          if(user1->following.at(i) == username2){
            following = true;
            followingindex = i;
            for(int j = 0; j <user2->followers.size(); j++){
              if(user2->followers.at(j) == username1){
                followerindex = j;
              }
            }
          }
        }

        if(following){
          msg = "Successfully unfollowed";
          user1->following.erase(user1->following.begin()+followingindex);
          user2->followers.erase(user2->followers.begin()+followerindex);
          
        }
        else{
          msg = "Error: not following";
        }
    }

    reply->set_msg(msg);
  


    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------

    //Get the username of the user logging in
    std::string loginUser = request->username();

    //Check to see if the username already exists in the server
    bool userExists = false;
    User existing; 
    for(int i = 0; i < users.size(); i++){
      if(loginUser == users.at(i).username){
        userExists = true;
        existing = users.at(i);
      }
    }
    //If yes, check to see if that user is online.  If online, send error message
    std::string msg;
    if(userExists){
        if(existing.online){
          msg = "Error: user already online";
        }
        else{
          existing.online = true;
          msg = "Login successful";
        }
    }
    //If no, create the user, send success messgae
    else{
      User newUser;
      newUser.username = loginUser;
      newUser.online = true;
      users.push_back(newUser);
      msg = "Login successful";
    }

    reply->set_msg(msg);
    return Status::OK;
  }

  //I have not gotten the timeline function to work correctly, It will send one message across all timelines
  //But then any subsequent messages result in a seg fault.
  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    
    Message msg;
    

    while(stream->Read(&msg)){
      std::string username = msg.username();
      int userindex = get_user_index(username);
      std::string outfile = username + "_timeline.txt";

      //Send messages to the user's timeline file
      
      if(msg.msg() == "Timeline dump"){
        std::vector<std::string> recentPosts;
        Message replyMsg;
        int currentPost = 0;
        std::ifstream ifs;
        ifs.open(outfile, std::ifstream::in);
        std::string currline;
        
        //Push all lines into a vector, then iterate to the 20 most recent
        while(getline(ifs, currline)){
              recentPosts.push_back(currline);
        }
        int starting_pos = 0;
        if(recentPosts.size() > 20){
          starting_pos = recentPosts.size()-20;
        }

        //Send these posts to the client
        if(users.at(userindex).stream == 0){
          users.at(userindex).stream = stream;
        }
        for(int i = starting_pos; i < recentPosts.size(); i++){
            replyMsg.set_msg(recentPosts.at(i));
            users.at(i).stream->Write(replyMsg);
        }
        
      }
      else{
        std::ofstream userTimeline;
        userTimeline.open(outfile, std::ios_base::app);
        google::protobuf::Timestamp msgtime = msg.timestamp();
        std::string time = google::protobuf::util::TimeUtil::ToString(msgtime);

        std::string timelinePost = msg.username() + "(" + time + ") >> " + msg.msg();
        userTimeline << timelinePost;
        userTimeline.close();

        //Send the post to all followers
        for(int i = 0; i < users.size(); i++){
            for(int j = 0; j < users.at(userindex).followers.size(); i++){
                //Make sure post is only sent to followers
                if(users.at(i).username == users.at(userindex).followers.at(j)){
                  //This section is responsible for putting the post in each follower's timeline file.
                  std::string follower_username = users.at(userindex).followers.at(j);
                  int follower_index = get_user_index(follower_username);
                  std::string follower_outfile = follower_username + "_timeline.txt";
                  std::ofstream follower_Timeline;
                  follower_Timeline.open(follower_outfile, std::ios_base::app);

                  follower_Timeline << timelinePost;
                  follower_Timeline.close();

                  if(users.at(follower_index).online && users.at(follower_index).stream !=0){
                    users.at(follower_index).stream->Write(msg);
                  }
                }
            }
        }

      }


    }
    
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a partFicular
  // port number.
  // ------------------------------------------------------------
    SNSServiceImpl sns;
    //Write code for making server data persistent once everything else is tested
    //std::ifstream infile;
    //infile.open("users.txt");
    ServerBuilder build;
    std::string address = "0.0.0.0:" + port_no;

    //Create grpc server
    build.AddListeningPort(address, grpc::InsecureServerCredentials());
    build.RegisterService(&sns);

    std::unique_ptr<Server> server(build.BuildAndStart());
    
    server->Wait();

}

int main(int argc, char** argv) {
  
  std::string port;
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}
