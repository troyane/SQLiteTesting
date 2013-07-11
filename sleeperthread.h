#ifndef SLEEPERTHREAD_H
#define SLEEPERTHREAD_H

#include <QThread>

class SleeperThread : public QThread
{
    Q_OBJECT
public:
    static void msSleep(uint ms) {
        if ( !ms ) { return; }

        SleeperThread sleeper(ms);
        sleeper.run();
    }
    explicit SleeperThread(uint ms) : m_sleepTime(ms) {}
    virtual void run() { msleep( m_sleepTime );}
    void setSleepTime(uint ms) { m_sleepTime = ms ;}
    uint sleepTime() const { return m_sleepTime; }
private:
    uint m_sleepTime;
};
#endif // SLEEPERTHREAD_H
