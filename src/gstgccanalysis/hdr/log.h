#ifndef LOG_H
#define LOG_H

#include <fstream>
#include <vector>
#include <string>
template<class T>
class log
{
private:
    std::ofstream out;
public:
    log();
    void setFileName(std::string filename);
    void setHeader(std::vector<std::string> headers);
    void logging(std::vector<T> data);
    ~log();

};

#endif // LOG_H
