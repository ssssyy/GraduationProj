/* SnowFlake implements snowflake algo to generate snowflake id*/

#include "SnowFlake.h"

int64_t SnowFlake::UniqueID(){
    int64_t now = TimeMs();
    if(now==m_lastTs){
        m_seq = (m_seq+1)&m_seqMask;
        if(m_seq==0){
            now = NextMs(m_lastTs);
        }
    }else{
        m_seq = 0;
    }
    m_lastTs = now;
    int64_t uid = (now-m_epoch)<<20 | m_machine<<10 | m_seq;
    return uid;
}

int64_t SnowFlake::TimeMs() const {
    auto time_now = std::chrono::system_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch());
    return duration_ms.count();
}

int64_t SnowFlake::NextMs(int64_t lastTs) const {
    int64_t ts = TimeMs();
    while(ts<=lastTs){
        ts = TimeMs();
    }
    return ts;
}