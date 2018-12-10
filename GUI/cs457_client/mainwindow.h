#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <mutex>
#include <queue>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <QTimer>


#include "client_globals.h"
#include "buffer.h"
#include "string_ops.h"
#include "tcp_client_socket.h"


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void log_only(std::string output);
    int load_config_file(std::string config_filepath, std::string& serverIP, int& port, std::string& nickname, bool& debug_enabled);
    void print_and_log(std::string output);
    std::string next_message();
    void communicate_with_server(IRC_Client::TCPClientSocket* clientSocket);
    void run_test_file(std::string test_filepath);
    std::string extract_channel_from_message(std::string msg);
    std::string extract_message_contents(std::string msg);
    void messagesUpdated();

    std::tuple<std::string, bool> build_outgoing_message(std::string client_input, bool &running);
    std::string list_command();
    std::string join_command(std::string);
    std::string quit_command();
    std::string privmsg_command(std::string, bool &);
    std::string help_command();
    std::string ping_command();
    std::string info_command();
    std::string time_command();
    std::string user_ip_command(std::string input, bool &should_send);
    std::string version_command();
    std::string users_command();
    std::string kill_command(std::string input, bool&);
    std::string kick_command(std::string, bool&);
    std::string user_host_command(std::string input, bool &should_send);

    bool RUNNING;
    bool should_log;
    bool communicator_running;
    std::string log_path;
    std::string current_channel;
    std::queue<std::string> message_queue;
    std::mutex message_queue_mutex;
    std::map<std::string, std::vector<std::string>> channel_messages;
    QTimer* timer;

private slots:
    void on_inputBox_returnPressed();

    void on_channelsList_itemDoubleClicked(QListWidgetItem *item);

    void display_current_channel();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
