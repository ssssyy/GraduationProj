/* SnowFlake implements snowflake algo to generate snowflake id*/

#include<cstdint>
#include<chrono>
#include<unistd.h>

class SnowFlake{
public:
    SnowFlake(){
        m_seqMask = 4095L;
        m_lastTs = TimeMs();
    }
    ~SnowFlake(){}
    int64_t UniqueID();
    int64_t TimeMs() const;
    int64_t NextMs(int64_t lastTs) const;
    void SetMachine(int64_t machineID){
        m_machine = machineID;
    }
    int64_t GetMachine(){
        return m_machine;
    }

private:
    int64_t m_seq{0};
    int64_t m_machine;
    int64_t m_lastTs{-1L};
    int64_t m_seqMask;
    int64_t m_epoch{1675958400000L};    // start ts 2023-02-10 00:00:00
};
