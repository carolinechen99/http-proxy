#include "MyProxy.h"

MyLog::MyLog()
{
    m_bIsInit = false;
}

MyLog::~MyLog()
{
    if (fstream::is_open())
        fstream::close();
    if (m_bIsInit)
        pthread_mutex_destroy(&m_mutex);
}

void MyLog::Output(const string& msg)
{
    pthread_mutex_lock(&m_mutex);
    cout << msg << endl;
    if (is_open())
    {
        write(msg.data(), msg.length());
        write("\r\n", 2);
        flush();
    }
    pthread_mutex_unlock(&m_mutex);
}

bool MyLog::Init(const string& path)
{
    fstream::open(path, ios::app | ios::out);
    if (pthread_mutex_init(&m_mutex, NULL))
        return false;
    m_bIsInit = true;
    return fstream::is_open();
}

void MyLog::Print(const string& msg, int unq_id)
{
    Output(to_string(unq_id) + ": " + msg);
}

void MyLog::PrintNote(const string& msg, int unq_id)
{
    Output(to_string(unq_id) + ": NOTE " + msg);
}

void MyLog::PrintError(const string& msg, int unq_id)
{
    Output(to_string(unq_id) + ": ERROR " + msg);
}

void MyLog::PrintWaning(const string& msg, int unq_id)
{
    Output(to_string(unq_id) + ": WARNING " + msg);
}
