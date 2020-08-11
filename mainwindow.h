#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPushButton>

struct DeviceUserData;
class MainWindow;
class QTableView;
static MainWindow* pMainWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *statistics;

    QString *getAllDevicesListStr();
    bool removeUserFromSystem(int uscurrid);
    void dropAllUsers();
    DeviceUserData createNewDeviceDataForName(char *name,char *longtd,char *latit);
    void updateDataForUser(int usid,const char *longtd,const char *latit);

public slots:
    void startServer();
    void stopServer();
    void statistic();

protected:
    int generateNewUserId();
    void removeUnusedRecords();

private:
    QList<DeviceUserData> m_devices;
    QMutex m_mutex;
    QTableView *m_activeUsers;
};

#endif // MAINWINDOW_H
