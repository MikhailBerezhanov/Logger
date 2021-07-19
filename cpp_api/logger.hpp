
#ifndef _LOGGER_HPP
#define _LOGGER_HPP

#include <cstdint>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <mutex>
#include <string>
#include <algorithm>
#include <typeinfo>

// Название модуля при подключении логера для штампа сообщений
#ifdef LOG_MODULE_NAME
	#define MODULE_NAME 	LOG_MODULE_NAME	
#else
	#define MODULE_NAME 	""
#endif

// Уровень сообщения для вывода
using log_lvl_t = uint8_t;
// Колбек ф-ия переполнения лог файла
using log_file_rotate_cb = void (*) (void* );

// Максимальный допустимый размер лог файла для хранения [Байт]
#define LOG_FILE_MAX_SIZE 	((uint64_t)(2 * 1048576))		// 2 [MB]

// Коды цветов - подсветки терминала
#define _RED     			"\x1b[31m"
#define _GREEN   			"\x1b[32m"
#define _YELLOW  			"\x1b[33m"
#define _BLUE    			"\x1b[34m"
#define _MAGENTA 			"\x1b[35m"
#define _CYAN    			"\x1b[36m"
#define _LGRAY   			"\x1b[90m"
#define _RESET   			"\x1b[0m"
#define _BOLD 	 			"\x1b[1m"

// Флаги - Поддержимаевые Уровни сообщений для вывода
#define MSG_SILENT			((log_lvl_t)(0))
#define MSG_ERROR			((log_lvl_t)(1))
#define MSG_DEBUG			((log_lvl_t)(2))
#define MSG_VERBOSE			((log_lvl_t)(3))
#define MSG_TRACE			((log_lvl_t)(4))
#define MSG_OVERTRACE		((log_lvl_t)(5))

#define LOG_LVL_BIT_MASK	((log_lvl_t)(0x7F))

// Флаг записи лога в файл
#define MSG_TO_FILE			((log_lvl_t)(1 << 7))

#define LOG_LVL_DEFAULT		MSG_ERROR

// Функциональный макрос Вывод отладочного сообщения с меткой времени и даты
#define log_msg(flags, str...)	Log::msg(Log::dtime_stamp, MODULE_NAME, flags, str)

// Функциональный макрос для формирования сообщения в месте возниковения исключения
#define excp_msg(str) ( (std::string)_RED + "Exception " + _BOLD + \
__func__ + "():" + std::to_string(__LINE__) + _RESET + ": " + (str) )

// Функциональный макрос для логированя с подсветкой критических ошибок
#define log_err(str...)	do{ \
	std::unique_lock<std::recursive_mutex> lock(Log::log_print_mutex); \
	Log::msg(Log::dtime_stamp, MODULE_NAME, MSG_ERROR | MSG_TO_FILE, _RED "Error in " _BOLD "%s %s():%d " _RESET, __FILE__, __func__, __LINE__ ); \
	Log::msg(Log::no_stamp, MODULE_NAME, MSG_ERROR | MSG_TO_FILE, str); \
}while(0)

// Функциональный макрос для логированя с подсветкой системных ошибок с описанием
#define log_perr(str...) do{ \
	std::unique_lock<std::recursive_mutex> lock(Log::log_print_mutex); \
	Log::msg(Log::dtime_stamp, MODULE_NAME, MSG_ERROR | MSG_TO_FILE, _RED "Perror in " _BOLD "%s %s():%d " _RESET, __FILE__, __func__, __LINE__); \
	Log::msg(Log::no_stamp, MODULE_NAME, MSG_ERROR | MSG_TO_FILE, str); \
	Log::msg(Log::no_stamp, MODULE_NAME, MSG_ERROR | MSG_TO_FILE, ": %s\n", strerror(errno)); \
}while(0)


// Шаблон - обертка для представляения с++ типов в виде с-типов
template<typename T>
inline auto to_c(T&& arg) -> decltype(std::forward<T>(arg)) 
{
    return std::forward<T>(arg);
}
// Перегрузка шаблона для поддержки вывода std::string как С-строки по ключу %s
inline const char* to_c(const std::string& s) { return s.c_str(); }
inline const char* to_c(std::string& s) { return s.c_str(); }
inline const char* to_c(const bool& b) { return b ? "True" : "False"; }
inline const char* to_c(bool& b) { return b ? "True" : "False"; }


// Класс-Интерфейс для управления логированием
class Log
{
public:

	typedef enum {
		no_stamp = 0,
		dtime_stamp,
		time_stamp,
		ms_stamp,
	}stamp_t;

	// Установка текущего уровня логирования, параметров лог файла
	static void init(log_lvl_t lvl, const std::string& name, uint64_t size = LOG_FILE_MAX_SIZE){
		log_lvl = lvl;
		log_fname = name;
		log_max_fsize = size;
	}

	// Получение текущего уровня логирования
	static log_lvl_t get_lvl(){
		std::lock_guard<std::mutex> lock(log_lvl_mutex);
		return log_lvl;
	}

	// Установка нового уровня логирования
	static void set_lvl(log_lvl_t new_lvl){
		std::lock_guard<std::mutex> lock(log_lvl_mutex);
		log_lvl = new_lvl;
	}

	// Проверка необходимости подготовки сообщения для вывода
	static bool check_lvl(log_lvl_t flags){
		log_lvl_t curr_lvl = get_lvl();
		log_lvl_t msg_lvl = flags & LOG_LVL_BIT_MASK;
		
		if(!(flags & MSG_TO_FILE) && (curr_lvl < msg_lvl)) return false;

		return true;
	}

	// Запись сообщения в лог-файл
	template<typename... Args>
	static int to_file(const char* stamp, const char* fmt, Args&&... args);

	// Шаблонная версия форматированного вывода с переменным кол-вом параметров
	template<typename... Args>
	static int msg(stamp_t st, const char* mod_name, log_lvl_t flags, const char* fmt, Args&&... args);

	// Перегрузка для поддержки неформатированного вывода без предупреждений компилятора
	static int msg(stamp_t st, const char* mod_name, log_lvl_t flags, const char* str){
		return msg(st, mod_name, flags, "%s", str);
	}

	static int msg(stamp_t st, const char* mod_name, log_lvl_t flags, const std::string& str){
		return msg(st, mod_name, flags, "%s", str);
	}

	// Дамп блока памяти в 16-ричном формате
	static void hex_dump(log_lvl_t flags, const uint8_t* buf, size_t len, const char* msg = "", const char* module_name = MODULE_NAME);

	// Формирование штампа сообщения
	static std::string make_msg_stamp(stamp_t type, const char* module_name);

	// мьютекс для целостного вывода комбинированных сообщений и доступа к лог-файлу
	static std::recursive_mutex log_print_mutex; 
	static std::recursive_timed_mutex log_file_mutex; 

private:
	static log_lvl_t log_lvl;				// текущий уровень логирования
	static std::mutex log_lvl_mutex;		// мьютекс доступа к текущему уровню логирования
	static std::string log_fname;			// имя лог-файла
	static uint64_t log_max_fsize;			// максимальный допустимый размер лог-файла [Байт]
	static log_file_rotate_cb log_rotate;	// колбек переполнения максимального размера лог-файта
	static void* log_rotate_arg;			// параметр  колбек ф-ии переполнения лог-файла

	// Получение текущего размера лог-файла
	static uint64_t get_file_size (const std::string& fname);
};


template<typename... Args>
int Log::to_file(const char* stamp, const char* fmt, Args&&... args) 
{
	if(log_fname == "") return 0;

	int ret = 0;
	std::FILE* logfp = nullptr;
	bool rotated = false;
	uint64_t file_size = 0;

	// Синхронизировать доступ к файлу с таймаутом 10 мс (файл может быть недоступен)
	std::unique_lock<std::recursive_timed_mutex> lock(log_file_mutex, std::defer_lock);
	if(!lock.try_lock_for(std::chrono::milliseconds(10))) return 0;

	try{
		file_size = get_file_size(log_fname);
	}
	catch(const std::runtime_error& e){
		std::cerr << e.what() << std::endl;
	}

	// Если задан максимальный размер и он превышен
    if (log_max_fsize && file_size >= log_max_fsize){
        log_msg(MSG_VERBOSE, "------ Rotating log file ------\n");
        if(log_rotate) log_rotate(log_rotate_arg);
        logfp = fopen(log_fname.c_str(), "w+");
        rotated = true;
    }
    else logfp = std::fopen(log_fname.c_str(), "a+");

    if(logfp){
    	if(rotated) std::fprintf(logfp, "----- %s -----\n", "Log file has been rotated");
    	if(stamp) std::fprintf(logfp, "%s", stamp);
        ret = std::fprintf(logfp, fmt, to_c(args)...); 
        std::fclose(logfp);
        
    }
   	else {
   		log_perr("Couldnt open log file '%s'", log_fname); 
   	}  

   	return ret;
}

// ПРИМ: Из-за того, что экземпляры ф-ии могут генерироваться в модулях с разными названиям
// нельзя использовать макрос MODULE_NAME внутри самой ф-ии, чтобы имя модуля не "захардкодилось"
// в конкретном экземпляре. Поэтому был добавлен параметр mod_name.
template<typename... Args>
int Log::msg(stamp_t st, const char* mod_name, log_lvl_t flags, const char* fmt, Args&&... args) 
{
	// Если не предоставлено имя модуля
	if(!strcmp(mod_name, "")) return 0;

	int ret = 0;

	log_lvl_t curr_lvl = get_lvl();
	log_lvl_t msg_lvl = flags & LOG_LVL_BIT_MASK;
	// Проверка необходимости подготовки сообщения для вывода
	if(!(flags & MSG_TO_FILE) && (curr_lvl < msg_lvl)) return 0;

	// Создание форматированной метаинформации о сообщении
	std::string msg_stamp = make_msg_stamp(st, mod_name);

	// Синхронизация вывода
	std::unique_lock<std::recursive_mutex> lock(log_print_mutex);

	// Проверка уровня сообщения для вывода в терминал
	// (игнорируем сообщения только для записи в файл и с уровнем выше заданного допустимого)
	if(msg_lvl && msg_lvl <= curr_lvl){
		std::printf("%s ", msg_stamp.c_str());
		ret = std::printf(fmt, to_c(args)...);
	}
	
	// Проверка необходимости записи сообщения в файл
	if(flags & MSG_TO_FILE){
		ret = to_file(msg_stamp.c_str(), fmt, args...);
	}

    return ret;
}

template <typename T>
const char* fmt_of(T arg)
{
    std::string arg_type = typeid(arg).name();

    log_msg(MSG_OVERTRACE, "%s arg_type: %s\n", __func__, arg_type);

    // Определение формата вывода для текущего параметра
    if(arg_type.find("basic_string") != std::string::npos) return "%s"; // std::string
    if(arg_type == "i") return "%d";    // int
    if(arg_type == "l") return "%ld";   // long
    if(arg_type == "x") return "%lld";  // long long
    if(arg_type == "j") return "%u";    // unsigned
    if(arg_type == "m") return "%lu";   // unsigned long
    if(arg_type == "y") return "%llu";  // unsigned long long
    if(arg_type == "f") return "%f";    // float
    if(arg_type == "d") return "%f";    // double
    if(arg_type == "e") return "%Lf";   // long double
    if(arg_type == "b") return "%s";    // bool

    return "";
}



#endif
