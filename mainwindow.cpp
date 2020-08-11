#include "mainwindow.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#include "mongoose.h"
#include <QDebug>
#include <QString>
#include <QFileInfo>
#include <QDateTime>
#include <QTableView>
#include <QStandardItemModel>

#define kUsedPortNumber "8083"
#define kMaximumUserDevices "10"
#define kMaxUserNameLength 50
#define kMaxCoordinatesLength 20
#define kMaxUserIDLength 10
#define kMaxUserDelayMsec 1000
#define kPermitionDropPassword "8083"

#define CONTENT_TYPE_JPEG	0
#define CONTENT_TYPE_TEXT	1
#define CONTENT_TYPE_XML    2

struct DeviceUserData
{
    int uniqID;
    char name[kMaxUserNameLength];
    char latitude[kMaxCoordinatesLength];
    char longitude[kMaxCoordinatesLength];
    long long int lastTimeRequest;
};
typedef struct DeviceUserData DeviceUserData;

static const char *HTTP_500 = "HTTP/1.1 500 Server Error\r\n\r\n";
static const char *HTTP_400 = "HTTP/1.1 400 Client Error\r\n\r\n";

struct mg_context *web_context;
static void* event_handler(enum mg_event event,
                                    struct mg_connection *conn);
static void SendResponse(struct mg_connection *conn, bool bOk, int nContentType, const char *strContent, int len);

static const char *options[] = {
      "listening_ports", kUsedPortNumber,
    "num_threads", kMaximumUserDevices,
    NULL };

static void redirect_to_ssl(struct mg_connection *conn,
                            const struct mg_request_info *request_info) {
    const char *p, *host = mg_get_header(conn, "Host");
    if (host != NULL && (p = strchr(host, ':')) != NULL) {
        mg_printf(conn, "HTTP/1.1 302 Found\r\n"
                  "Location: https://%.*s:8082/%s:8082\r\n\r\n",
                  (int) (p - host), host, request_info->uri);
    } else {
        mg_printf(conn, "%s", "HTTP/1.1 500 Error\r\n\r\nHost: header is not set");
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{       
    pMainWindow = this;
    startButton = new QPushButton("Start",this);
    stopButton = new QPushButton("Stop",this);
    statistics = new QPushButton("Statistic",this);

    startButton->move(10,10);
    stopButton->move(startButton->x()+startButton->width()+340,10);
    resize(startButton->x()+startButton->width()+350+stopButton->width(),
           400);
    statistics->move(this->width()*0.5-statistics->width()*0.5-5, 10);

    connect(startButton,SIGNAL(clicked()),this,SLOT(startServer()));
    connect(stopButton,SIGNAL(clicked()),this,SLOT(stopServer()));
    connect(statistics,SIGNAL(clicked()),this,SLOT(statistic()));

    m_activeUsers = new QTableView(this);
    m_activeUsers->move(10, startButton->y()+startButton->height()+10);
    m_activeUsers->resize(this->width()-20, this->height()-m_activeUsers->y()-10);
}

MainWindow::~MainWindow()
{
    stopServer();
}

void MainWindow::startServer()
{
    startButton->setEnabled(false);
    if ((web_context = mg_start(&event_handler, NULL, options)) == NULL) {
        printf("%s\n", "Cannot start chat server, fatal exit");
        exit(EXIT_FAILURE);
    }
}

void MainWindow::stopServer()
{
    m_devices.clear();
    if(web_context != NULL)
    {
        mg_stop(web_context);
        web_context = NULL;
    }
    startButton->setEnabled(true);
}

void MainWindow::statistic()
{
    QMutexLocker loc(&m_mutex);
    removeUnusedRecords();
    if(m_activeUsers->model())
        m_activeUsers->model()->deleteLater();
    QStandardItemModel *model = new QStandardItemModel(m_devices.count(),5,this);
    model->setHorizontalHeaderItem(0, new QStandardItem(QString("Number")));
    model->setHorizontalHeaderItem(1, new QStandardItem(QString("User ID")));
    model->setHorizontalHeaderItem(2, new QStandardItem(QString("Name")));
    model->setHorizontalHeaderItem(3, new QStandardItem(QString("Latitude")));
    model->setHorizontalHeaderItem(4, new QStandardItem(QString("Longitude")));

    for(int i=0; i<m_devices.count(); i++)
    {
        DeviceUserData data = m_devices.at(i);

        QStandardItem *col1 = new QStandardItem(QString::number(i+1));
        QStandardItem *col2 = new QStandardItem(QString::number(data.uniqID));
        QStandardItem *col3 = new QStandardItem(data.name);
        QStandardItem *col4 = new QStandardItem(data.latitude);
        QStandardItem *col5 = new QStandardItem(data.longitude);
        model->setItem(i,0,col1);
        model->setItem(i,1,col2);
        model->setItem(i,2,col3);
        model->setItem(i,3,col4);
        model->setItem(i,4,col5);
    }
    m_activeUsers->setModel(model);
}

int MainWindow::generateNewUserId()
{
    int maxID = 1;
    for(DeviceUserData data : m_devices)
    {
        if(maxID < data.uniqID)
        {
            maxID = data.uniqID;
        }
    }
    return ++maxID;
}

void MainWindow::removeUnusedRecords()
{
    bool unusedEmpty = false;
    while(!unusedEmpty)
    {
        unusedEmpty = true;
        for(int i=0; i<m_devices.count(); i++)
        {
            DeviceUserData data = m_devices.at(i);

            int userDelay = QDateTime::currentMSecsSinceEpoch() - data.lastTimeRequest;
            if(kMaxUserDelayMsec < userDelay)
            {
                m_devices.removeAt(i);
                unusedEmpty = false;
                break;
            }
        }
    }
}

QString *MainWindow::getAllDevicesListStr()
{
    QMutexLocker loc(&m_mutex);
    QString jsonList="{\"users\":[";
    for(int i=0; i<m_devices.count(); i++)
    {
        DeviceUserData data = m_devices.at(i);
        jsonList+="{";
        jsonList+="\"userID\":"+QString::number(data.uniqID)+",";
        jsonList+="\"name\":\""+QString(data.name)+"\",";
        jsonList+="\"longitude\":"+QString(data.longitude)+",";
        jsonList+="\"latitude\":"+QString(data.latitude);
        if(i+1==m_devices.count())
            jsonList+="}";
        else
            jsonList+="},";
    }
    jsonList+="]}";

    QString *answer = new QString(jsonList);
    return answer;
}

bool MainWindow::removeUserFromSystem(int uscurrid)
{
    bool success = false;
    QMutexLocker loc(&m_mutex);
    for(int i=0; i<m_devices.count(); i++)
    {
        DeviceUserData data = m_devices.at(i);
        if(uscurrid == data.uniqID)
        {
            success = true;
            m_devices.removeAt(i);
            break;
        }
    }

    return success;
}

void MainWindow::dropAllUsers()
{
    QMutexLocker loc(&m_mutex);
    m_devices.clear();
}

DeviceUserData MainWindow::createNewDeviceDataForName(char *name,char *longtd,char *latit)
{
    QMutexLocker loc(&m_mutex);
    DeviceUserData data;
    strcpy(data.name,name);
    strcpy(data.longitude,longtd);
    strcpy(data.latitude,latit);
    data.uniqID = generateNewUserId();
    data.lastTimeRequest = QDateTime::currentMSecsSinceEpoch();

    m_devices.append(data);
    return data;
}

void MainWindow::updateDataForUser(int usid,const char *longtd,const char *latit)
{
    QMutexLocker loc(&m_mutex);   
    removeUnusedRecords();

    for(int i=0; i<m_devices.count(); i++)
    {
        DeviceUserData data = m_devices.at(i);
        if(usid == data.uniqID)
        {
            strcpy(m_devices[i].longitude,longtd);
            strcpy(m_devices[i].latitude,latit);           
            m_devices[i].lastTimeRequest = QDateTime::currentMSecsSinceEpoch();
        }
    }
}

static void *event_handler(enum mg_event event, struct mg_connection *conn)
{
    const struct mg_request_info *request_info = mg_get_request_info(conn);
    void *processed = (void*)1;

    if (event == MG_NEW_REQUEST) {

        QString reqinf_uri(request_info->uri);

        qDebug() << reqinf_uri;

        if(!strcmp(reqinf_uri.toStdString().c_str(), "/register"))
        {
            char nameUser[kMaxUserNameLength];
            char latitude[kMaxCoordinatesLength];
            char longitude[kMaxCoordinatesLength];            

            QString query(request_info->query_string);
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "name", nameUser, sizeof(nameUser));
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "latitude", latitude, sizeof(latitude));
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "longitude", longitude, sizeof(longitude));

            qDebug() << query;
            qDebug() << "name:    " << nameUser << "latitude:    " << latitude << "longitude:    " << longitude;

            if(strlen(nameUser)>0)
            {
                DeviceUserData deviceData = pMainWindow->createNewDeviceDataForName(nameUser,longitude,latitude);
                QString answer = QString("{\"success\":true,\"userID\":%1}").arg(deviceData.uniqID);

                SendResponse(conn, true, CONTENT_TYPE_TEXT, answer.toStdString().c_str(), strlen(answer.toStdString().c_str()));
                qDebug() << "id:    " << answer;
            }
            else
            {
                mg_printf(conn, "%s%s", HTTP_400, "Invalid User Name");
            }
        }
        else if(!strcmp(reqinf_uri.toStdString().c_str(), "/drop_all_users"))
        {
            char pswStr[kMaxUserIDLength];
            QString query(request_info->query_string);
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "psw", pswStr, sizeof(pswStr));
            QString answer("{\"success\":false}");

            if(!strcmp(pswStr,kPermitionDropPassword))
            {
                pMainWindow->dropAllUsers();
                answer = "{\"success\":true}";
            }
            SendResponse(conn, true, CONTENT_TYPE_TEXT, answer.toStdString().c_str(), strlen(answer.toStdString().c_str()));

        }
        else if(!strcmp(reqinf_uri.toStdString().c_str(), "/user_leave_system"))
        {
            char currUserID[kMaxUserIDLength];
            QString query(request_info->query_string);

            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "userID", currUserID, sizeof(currUserID));
            qDebug() << "user_leave_system:   " << currUserID;

            if(!strlen(currUserID))
            {
                mg_printf(conn, "%s%s", HTTP_400, "Invalid User ID");
            }
            else
            {
                int uscurrid = strtol(currUserID, NULL, 10);
                if(uscurrid>0)
                {
                    if(pMainWindow->removeUserFromSystem(uscurrid))
                    {
                        QString answer("{\"success\":true}");
                        SendResponse(conn, true, CONTENT_TYPE_TEXT, answer.toStdString().c_str(), strlen(answer.toStdString().c_str()));
                    }
                    else
                    {
                        mg_printf(conn, "%s%s", HTTP_500, "Invalid remove user oparation");
                    }
                }
            }
        }
        else if(!strcmp(reqinf_uri.toStdString().c_str(), "/update_location_data"))
        {
            char nameUser[kMaxUserNameLength];
            char latitude[kMaxCoordinatesLength];
            char longitude[kMaxCoordinatesLength];
            char currUserID[kMaxUserIDLength];

            QString query(request_info->query_string);

            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "userID", currUserID, sizeof(currUserID));
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "name", nameUser, sizeof(nameUser));
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "latitude", latitude, sizeof(latitude));
            mg_get_var(query.toStdString().c_str(), strlen(query.toStdString().c_str()), "longitude", longitude, sizeof(longitude));

            qDebug() << "id:" << currUserID << "name:    " << nameUser << "latitude:    " << latitude << "longitude:    " << longitude;

            if(!strlen(currUserID) || !strlen(nameUser) || !strlen(latitude) || !strlen(longitude))
            {
                mg_printf(conn, "%s%s", HTTP_400, "Invalid User Data");
            }
            else
            {
                int uscurrid = strtol(currUserID, NULL, 10);
                if(uscurrid>0)
                {
                    pMainWindow->updateDataForUser(uscurrid,longitude,latitude);
                }
                QString *answer = pMainWindow->getAllDevicesListStr();
                if(answer)
                {
                    SendResponse(conn, true, CONTENT_TYPE_TEXT, answer->toStdString().c_str(), strlen(answer->toStdString().c_str()));
                    qDebug() << *answer;
                    delete answer;
                }
                answer = NULL;
            }
        }
        else
        {
            processed = NULL;
        }
    }

    return processed;
}

static void SendResponse(struct mg_connection *conn, bool bOk, int nContentType, const char *strContent, int contlen)
{
    char bufStr[BUFSIZ];
    char *strResponse = bufStr;
    char *additChar;
    int resplen = 0;

    strcpy(strResponse, "HTTP/1.1 200 OK\r\nContent-Type: ");

    if (bOk)
    {
        switch (nContentType)
        {
            case CONTENT_TYPE_TEXT:
                strcat(strResponse, "text/html\r\n");
                break;
            case CONTENT_TYPE_JPEG:
                strcat(strResponse, "image/jpeg\r\n");
                break;
            case CONTENT_TYPE_XML:
                strcat(strResponse, "text/xml\r\n");
                break;
        }

        strcat(strResponse, "Connection: Keep-Alive\r\nCache-Control: no-cache\r\n");

        additChar = strrchr(strResponse, '\n');
        additChar++;
        strResponse += (int)(additChar - strResponse);

        if(!strContent)
            contlen = 0;

        _snprintf(strResponse, BUFSIZ -(int)(additChar - strResponse), "Content-Length: %d\r\n\r\n", contlen);

        strResponse = bufStr;
        resplen = strlen(strResponse)*sizeof(strResponse[0]);
        mg_write(conn, (const void *) strResponse, resplen);

        if (contlen > 0)
            mg_write(conn, (const void *) strContent, contlen);
    }
}
