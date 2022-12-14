#include <ctime>
#include <sys/stat.h>
#include <sstream>
#include <iterator>

#include "logger.hpp"

// Инициализация статических членов класса
std::recursive_mutex Logging::log_print_mutex;
// std::recursive_timed_mutex Logging::log_file_mutex;


// Формирование штампа сообщения
std::string Logging::make_msg_stamp(stamp_t type, const std::string &module_name, const char *fmt)
{
	if(type == no_stamp) return "";

	struct tm timeinfo;
	struct timespec spec;
 
	clock_gettime(CLOCK_REALTIME, &spec);

	if(!localtime_r(&spec.tv_sec, &timeinfo)){
		return "localtime_r failed";
	}

	char time_str[64] = {0};

	switch(type){
        case only_time: strftime(time_str, sizeof time_str, "[ %T ]", &timeinfo); break;
        case ms_time: {
            strftime(time_str, sizeof time_str, "%d.%m.%y %T", &timeinfo); 
            char buf[80] = {0};
            snprintf(buf, sizeof buf, "[ %s.%03lu ]%s ", time_str, spec.tv_nsec / 1000000L, module_name.c_str());
            std::string s {buf};
            return s;
        }

        case custom: strftime(time_str, sizeof time_str, fmt, &timeinfo); break;

        case date_time: 
        default:
            strftime(time_str, sizeof time_str, "[ %d.%m.%y %T ]", &timeinfo); break;
    }

	std::string s {time_str};
	s += module_name + " "; 

	return s;
}

// Получение текущего размера лог-файла
uint64_t Logging::get_file_size (const std::string& fname)
{
    struct stat st;
    if (0 != stat(fname.c_str(), &st)) return 0;

    return (uint64_t)st.st_size;
}

// Дамп блока памяти в 16-ричном формате
void Logging::hex_dump(log_lvl_t flags, const char *buf, size_t len, const std::string &msg_str, uint8_t delim)
{
	hex_dump(flags, reinterpret_cast<const uint8_t*>(buf), len, msg_str, delim);
}

void Logging::hex_dump(log_lvl_t flags, const uint8_t *buf, size_t len, const std::string &msg_str, uint8_t delim)
{
	if( !check_lvl(flags) ){
		return;
	} 

	// Синхронизация вывода
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex);

	Logging::stamp_t tmp = this->get_time_stamp();
	msg(flags, "%s",  msg_str);	
	this->set_time_stamp(Logging::no_stamp);
	for(size_t i = 0; i < len; ++i){
		if(i % delim == 0) msg(flags, "\n\t");
		msg(flags, "%02X%s", buf[i], (((i + 1) % 8) == 0) ? "  " : " ");
	}
	msg(flags, "\n");
	this->set_time_stamp(tmp);
}

std::string Logging::padding(int col_size, const std::string &s, const char pad)
{
	// Проверяем используют ASCII (однобайтовые) символы или же двухбайтовые 
	int two_bytes_char = 0;
	for(const unsigned char &c : s){
		if((c == 0xD0) || (c == 0xD1)){
			++two_bytes_char;
		}
	}

	int pad_num = col_size - s.size() + two_bytes_char;
	if(pad_num < 0){
		return s;
	} 

	std::string::size_type indent_left = pad_num / 2;
	std::string::size_type indent_right(indent_left);
	if(pad_num % 2){
		++indent_right;
	} 

	return (std::string(indent_left, pad) + s + std::string(indent_right, pad));
}

// Для использования одного экземпляра логгера в нескольких файлах проекта
#ifdef _SHARED_LOG
Logging logger;
#endif


#ifdef _LOGGER_TEST
#include <thread>

void printer(Logging *obj, int num, int thread_no)
{
	for(int i = 0; i < num; ++i){
		obj->msg(MSG_DEBUG | MSG_TO_FILE, "(TH.%d) #%d debug + to_file message\n", thread_no, i);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

    return;
}

void printer_error(Logging *obj, int num, int thread_no)
{
	for(int i = 0; i < num; ++i){
		logging_err(*obj, "(TH.%d) #%d error message\n", thread_no, i);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

    return;
}

int main(int argc, char* argv[])
{
	Logging logger(MSG_VERBOSE, "[ MYLOG ]");

	std::string s{"verbose msg"};

	logger.init(MSG_VERBOSE, "Log.log", 3, KB_to_B(2));

	logger.msg(MSG_VERBOSE, "MESSAGE CONST CHAR TEST\n");

	char buf[4] = {'d', 'b', 'g', 0};
	logger.msg(MSG_DEBUG, "%s\n", buf); 

	logger.msg(MSG_SILENT, "%s\n", "silent msg");

	logger.msg(MSG_TO_FILE, "to file msg\n");

	logger.msg(MSG_VERBOSE | MSG_TO_FILE, "%s\n", s);

	logger.msg(MSG_TRACE | MSG_TO_FILE, "%s\n", "MESSAGE TO FILE ONLY");

	s = "fucked_up";
	logging_err(logger, "string error test: %s\n", s);

	logging_err(logger, "const char error test\n");

	logging_perr(logger, "system call failed (%s)", s);

	char buff[40];
	memset(buff, 0xBE, sizeof (buff));
	logger.hex_dump(MSG_DEBUG, (uint8_t*)buff, sizeof (buff), "buff_hex: ");

	logger.msg(MSG_DEBUG, "%s\n", excp_func(std::string{"error description: "} + strerror(errno)));

	bool test_false = false;
	bool test_true = true;
	logger.msg(MSG_DEBUG, "Bool values: %s , %s\n", test_false, test_true);

	// int ival;
	// long lval;
	// long long llval;
	// unsigned uival;
	// unsigned long ulval;
	// unsigned long long ullval;
	// float fval;
	// double dval;
	// long double ldval;
	// bool bval;

	// fmt_of(s);   	// basic_string
	// fmt_of(ival);   	// i
	// fmt_of(lval);   	// l
	// fmt_of(llval);  	// x
	// fmt_of(uival);  	// j
	// fmt_of(ulval);  	// m
	// fmt_of(ullval); 	// y
	// fmt_of(fval);   	// f
	// fmt_of(dval);   	// d
	// fmt_of(ldval);  	// e
	// fmt_of(bval);   	// b

	logger.set_time_stamp(Logging::ms_time);
	
    logger.msg(MSG_DEBUG | MSG_TO_FILE, "Starting thread(s)\n");
    // std::unique_lock<std::recursive_timed_mutex> lock(Log::log_file_mutex);

    for (int i = 0; i < 4; ++i){
    	std::thread(printer, &logger, 10, i).detach();
    }
    std::thread(printer_error, &logger, 10, 10).detach();
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
	return 0;
}
#endif
