
#include <ctime>
#include <sys/stat.h>

#ifdef UNIT_TEST
	#define LOG_MODULE_NAME 	"[ LOGER ] "
#endif

#include "loger.hpp"

// Инициализация статических членов класса
log_lvl_t Log::log_lvl = LOG_LVL_DEFAULT;
std::mutex Log::log_lvl_mutex;
std::recursive_mutex Log::log_print_mutex;
std::recursive_timed_mutex Log::log_file_mutex;
std::string Log::log_fname = "";
uint64_t Log::log_max_fsize = LOG_FILE_MAX_SIZE;
log_file_rotate_cb Log::log_rotate = nullptr;
void* Log::log_rotate_arg = nullptr;



// Формирование штампа сообщения
std::string Log::make_msg_stamp(stamp_t type, const char* module_name)
{
	if(type == no_stamp) return "";

	struct tm timeinfo;
	struct timespec spec;
 
	clock_gettime(CLOCK_REALTIME, &spec);

	if(!localtime_r(&spec.tv_sec, &timeinfo)){
		return "localtime_r failed";
	}

	char time_str[32] = {0};

	switch(type){
        case time_stamp: strftime(time_str, sizeof time_str, "[ %T ]", &timeinfo); break;
        case ms_stamp: {
            strftime(time_str, sizeof time_str, "%d.%m.%y %T", &timeinfo); 
            char buf[64] = {0};
            snprintf(buf, sizeof buf, "[ %s.%03lu ] %s", time_str, spec.tv_nsec / 1000000L, module_name);
            std::string s {buf};
            return s;
        }

        case dtime_stamp: 
        default:
            strftime(time_str, sizeof time_str, "[ %d.%m.%y %T ]", &timeinfo); break;
    }


	std::string s {time_str};
	s += module_name; 

	return s;
}

// Получение текущего размера лог-файла
uint64_t Log::get_file_size (const std::string& fname)
{
    struct stat st;
    if (0 != stat(fname.c_str(), &st)) 
    	throw std::runtime_error("File '" + fname + "' stat failed: " + strerror(errno));

    return (uint64_t)st.st_size;
}

// Дамп блока памяти в 16-ричном формате
void Log::hex_dump(log_lvl_t flags, const uint8_t* buf, size_t len, const char* msg_str, const char* module_name)
{
	if(!check_lvl(flags)) return;

	// Синхронизация вывода
	std::unique_lock<std::recursive_mutex> lock(log_print_mutex);

	msg(dtime_stamp, module_name, flags, "%s\n\t",  msg_str);
	for(size_t i = 0; i < len; ++i){
		if(i && i % 16 == 0) msg(no_stamp, module_name, flags, "\n\t");
		msg(no_stamp, module_name, flags, "%02X ", buf[i]);
	}
	msg(no_stamp, module_name, flags, "\n");
}

// TODO:
void unformatted()
{
	std::cout << Log::make_msg_stamp(Log::dtime_stamp, MODULE_NAME) << "Setting " << _BOLD << _RESET << " to: " << std::endl;

	//std::cout << Log::make_msg_stamp(LOG_MODULE_NAME) << "Config file json:\n" << std::setw(4) << "json" << std::endl;
}

#ifdef UNIT_TEST
#include <thread>

void printer(int num)
{
	for(int i = 0; i < num; ++i)
		Log::msg(Log::ms_stamp, MSG_DEBUG | MSG_TO_FILE, "Fucking file muted\n");

    return;
}

int main(int argc, char* argv[])
{
	std::string s{"verbose msg"};

	Log::init(MSG_VERBOSE, "Log.log", 1000);

	log_msg(MSG_VERBOSE, "MESSAGE CONST CHAR TEST\n");

	char buf[4] = {'d', 'b', 'g', 0};
	log_msg(MSG_DEBUG, "%s\n", buf); 

	log_msg(MSG_SILENT, "%s\n", "silent msg");

	log_msg(MSG_TO_FILE, "to file msg\n");

	log_msg(MSG_VERBOSE | MSG_TO_FILE, "%s\n", s);

	log_msg(MSG_TRACE | MSG_TO_FILE, "%s\n", "MESSAGE TO FILE ONLY");

	s = "fucked_up";
	log_err("string error test: %s\n", s);

	log_err("const char error test\n");

	log_perr("system call failed: %s\n", s.c_str());

	char buff[40];
	//memset(buff, 0xBE, sizeof (buff));
	Log::hex_dump(MSG_DEBUG, (uint8_t*)buff, sizeof (buff), "buff_hex: ");

	log_msg(MSG_DEBUG, "%s\n", excp_msg(std::string{"error description: "} + strerror(errno)));

	bool test_false = false;
	bool test_true = true;
	log_msg(MSG_DEBUG, "Bool values: %s , %s\n", test_false, test_true);

	int ival;
    long lval;
    long long llval;
    unsigned uival;
    unsigned long ulval;
    unsigned long long ullval;
    float fval;
    double dval;
    long double ldval;
    bool bval;

    fmt_of(s);   	// basic_string
    fmt_of(ival);   	// i
    fmt_of(lval);   	// l
    fmt_of(llval);  	// x
    fmt_of(uival);  	// j
    fmt_of(ulval);  	// m
    fmt_of(ullval); 	// y
    fmt_of(fval);   	// f
    fmt_of(dval);   	// d
    fmt_of(ldval);  	// e
    fmt_of(bval);   	// b

    
    std::unique_lock<std::recursive_timed_mutex> lock(Log::log_file_mutex);

    Log::msg(Log::ms_stamp, MSG_DEBUG, "Starting thread\n");
    std::thread(printer, 10).join();

	return 0;
}
#endif
