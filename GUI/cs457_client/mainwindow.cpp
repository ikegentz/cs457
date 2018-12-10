#include "mainwindow.h"
#include "ui_mainwindow.h"

#include<iostream>
#include <string>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->RUNNING = true;
    this->should_log = false;
    this->communicator_running = true;
    log_path = IRC_Client::DEFAULT_LOGPATH;
    this->channel_messages["general"] = std::vector<std::string>();

    ui->channelsList->addItem("general");
    this->current_channel = "general";

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(display_current_channel()));
    timer->start(500);
    //QObject::connect(this, SIGNAL(messagesUpdated()), this, SLOT(display_current_channel()));

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_inputBox_returnPressed()
{
    std::string input = ui->inputBox->text().toStdString();

    std::transform(input.begin(), input.end(), input.begin(), ::tolower);

    std::string outgoing_msg;
    bool should_send;
    tie(outgoing_msg, should_send) = this->build_outgoing_message(input, this->RUNNING);

    // push a message onto the queue to be sent by the socket thread
    if(should_send)
    {
        std::lock_guard <std::mutex> guard(this->message_queue_mutex);
        this->message_queue.push(outgoing_msg);


        std::vector<std::string> cur_messages = this->channel_messages.find(this->current_channel)->second;
        std::cout << "ADDING MESSAGE"  << std::endl;

        cur_messages.push_back("ME: " + outgoing_msg);
        this->channel_messages[this->current_channel] = cur_messages;
        this->display_current_channel();

        //emit this->messagesUpdated();
        log_only(outgoing_msg);
    }

    ui->inputBox->clear();
}

void MainWindow::log_only(std::string output)
{
    if(should_log)
    {
        std::fstream fs;
        fs.open (log_path, std::fstream::out | std::fstream::app);

        // append to log file
        fs << output << std::endl;
        fs.close();
    }
}

int MainWindow::load_config_file(std::string config_filepath, std::string& serverIP, int& port, std::string& nickname, bool& debug_enabled)
{
    std::ifstream in(config_filepath);

    if(!in) {
        std::cout << "[CLIENT] Couldn't open config file. Running with defaults and command line args" << std::endl;
        return 1;
    }

    std::cout << "[CLIENT] Loaded options from " << config_filepath << std::endl;

    std::string str;
    while (std::getline(in, str)) {
        // skip comments
        if((str.find("#") != std::string::npos) || (str.size() <= 1))
            continue;

        std::vector<std::string> tokens;
        Utils::tokenize_line(str, tokens);

        if(tokens.size() != 2)
            continue;

        if(tokens[0].find("last_server_used") != std::string::npos)
        {
            serverIP = tokens[1];
            std::cout << "[CLIENT] Server IP from config file: " << serverIP << std::endl;
        }
        else if(tokens[0].find("port") != std::string::npos)
        {
            port = stoi(tokens[1]);
            std::cout << "[CLIENT] Port from config file: " << tokens[1] << std::endl;
        }
        else if(tokens[0].find("default_debug_mode") != std::string::npos)
        {
            std::transform(tokens[1].begin(), tokens[1].end(), tokens[1].begin(), ::tolower);

            if(tokens[1].find("true") != std::string::npos)
                debug_enabled = true;
            else debug_enabled = false;

            std::cout << "[CLIENT] Default debug mode from config file: " << debug_enabled << std::endl;
        }
        else if(tokens[0].find("default_log_file") != std::string::npos)
        {
            log_path = tokens[1];
            std::cout << "[CLIENT] Log path specified in config file: " << log_path << std::endl;
        }
        else if(tokens[0].compare("log") == 0)
        {
            std::transform(tokens[1].begin(), tokens[1].end(), tokens[1].begin(), ::tolower);

            if(tokens[1].find("true") != std::string::npos)
                should_log = true;
            else should_log = false;

            std::cout << "[CLIENT] Will log this sesssion? (specified in config file): " << should_log << std::endl;
        }
        else if(tokens[0].find("nickname") != std::string::npos)
        {
            nickname = tokens[1];
            std::cout << "[CLIENT] Nickname specified in config file: " << nickname << std::endl;
        }
    }
    return 0;
}

void MainWindow::print_and_log(std::string output)
{
    if(should_log)
    {
        std::fstream fs;
        fs.open (log_path, std::fstream::out | std::fstream::app);

        // append to log file
        fs << output << std::endl;
        fs.close();
    }

    // do the actual print to terminal
    std::cout << output << std::endl;
}

// we only want to use the mutex lock for the pop(), so put it in a function so it goes out of scope
std::string MainWindow::next_message()
{
    std::lock_guard<std::mutex> guard(this->message_queue_mutex);
    std::string message = this->message_queue.front(); // get the message
    this->message_queue.pop(); // remove it

    return message;
}

std::string MainWindow::extract_channel_from_message(std::string msg)
{
    std::string start = "[CHANNEL]";
    std::string end = "[/CHANNEL]";

    int startPos = msg.find(start) + start.size();
    int endPos = msg.find(end);

    return msg.substr(startPos, endPos - (end.size()-1));
}

std::string MainWindow::extract_message_contents(std::string msg)
{
    std::string end = "[/CHANNEL]";

    return msg.substr(msg.find(end) + end.size());
}

void MainWindow::communicate_with_server(IRC_Client::TCPClientSocket* clientSocket)
{
    // process any messages that haven't been sent yet
    while(this->communicator_running || !this->message_queue.empty())
    {
        // check if there's any new messages from the server
        ssize_t v;
        std::string msg;
        // this includes a timeout so we don't get stuck here for too long if we need to send something
        tie(msg, v) = clientSocket->recvString(4096, false);

        // print response from server, as long as it isn't just an empty acknowledgement
        if(v > 0 && msg != "\n")
        {
            print_and_log(msg);

            // add server command to display, regardless of current channel
            // this '!' is decieving, but remember that this returns an index, and any non-zero is true
            if(!msg.find("[SERVER]"))
                std::cerr << "TODO -- Display server message regardless of channel" << std::endl;


            std::string channel_name = this->extract_channel_from_message(msg);
            if(this->channel_messages.find(channel_name) == this->channel_messages.end())
                log_only("Not subscribed to channel" + channel_name + ". Skipping message");

            // add the message to our queue of messages
            if(this->channel_messages.find(channel_name) != this->channel_messages.end())
            {
                std::vector<std::string> cur_messages = this->channel_messages.find(channel_name)->second;
                std::cout << "ADDING MESSAGE"  << std::endl;
                cur_messages.push_back(this->extract_message_contents(msg));
                this->channel_messages[channel_name] = cur_messages;

                //this->display_current_channel();
            }
        }

        if(!this->message_queue.empty())
        {
            std::string message = next_message();
            clientSocket->sendString(message, false);
        }
    }
}

void MainWindow::run_test_file(std::string test_filepath)
{
    std::ifstream in(test_filepath);

    if(!in) {
        std::cout << "[CLIENT] Couldn't open test file" << std::endl;
        return;
    }

    std::cout << "[CLIENT] Running test file " << test_filepath << std::endl;

    bool RUNNING = true;
    std::string str;
    do
    {
        while (std::getline(in, str))
        {
            std::string outgoing_msg;
            bool should_send;
            tie(outgoing_msg, should_send) = this->build_outgoing_message(str, RUNNING);

            // push a message onto the queue to be sent by the socket thread
            if (should_send)
            {
                std::lock_guard <std::mutex> guard(this->message_queue_mutex);
                message_queue.push(outgoing_msg);
                print_and_log(outgoing_msg);
            }

            // sleep for a while since we want to simulate an actual user running commands
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }
    }while(RUNNING);
    communicator_running = false;
}

void MainWindow::on_channelsList_itemDoubleClicked(QListWidgetItem *item)
{
    std::string channel_name  = item->text().toStdString();
    this->current_channel = channel_name;
}

void MainWindow::display_current_channel()
{
    std::vector<std::string> message_list = this->channel_messages.find(this->current_channel)->second;

    std::string display;

    for(std::string cur : message_list)
        display += cur + "\n";

    ui->messageDisplay->clear();
    ui->messageDisplay->setPlainText(QString::fromStdString(display));
}

