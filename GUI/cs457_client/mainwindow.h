#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <mutex>
#include <queue>
#include <fstream>
#include <unistd.h>
#include <thread>


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

    bool RUNNING;
    bool should_log;
    bool communicator_running;
    std::string log_path;
    std::queue<std::string> message_queue;
    std::mutex message_queue_mutex;

private slots:
    void on_lineEdit_returnPressed();

    void on_inputBox_returnPressed();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
