
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
#include <map>
#include <vector>

// Название модуля логирования по умолчанию
#define LOGGER_NAME 		"[ LOGGER ]"

// Уровень сообщения для вывода
using log_lvl_t = uint8_t;
// Колбек ф-ия переполнения лог файла
using log_file_rotate_cb = void (*) (void* );

// МБайты -> Байты
constexpr uint64_t MB_to_B(uint64_t x) { return ((x) * 1048576); };
// КБайты -> Байты
constexpr uint64_t KB_to_B(uint64_t x) { return ((x) * 1024); };

// Максимальный допустимый размер лог файла для хранения [Байт]
#define LOG_FILE_MAX_SIZE 	( MB_to_B(2) )		// 2 [MB]
// Максимальное время ожидания блокировки лог-файла [мс]
#define LOG_FILE_LOCK_MS	10
// Максимальное число хранимых лог-файлов после ротации
#define LOG_FILE_MAX_NUM	3

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
#define MSG_WARNING			((log_lvl_t)(2))
#define MSG_INFO			((log_lvl_t)(3))
#define MSG_DEBUG			((log_lvl_t)(4))
#define MSG_VERBOSE			((log_lvl_t)(5))
#define MSG_TRACE			((log_lvl_t)(6))

#define LOG_LVL_BIT_MASK	((log_lvl_t)(0x7F))

// Флаг записи лога в файл
#define MSG_TO_FILE			((log_lvl_t)(1 << 7))

#define LOG_LVL_DEFAULT		MSG_ERROR

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
class Logging
{
public:

	typedef enum {
		no_stamp = 0,
		date_time,
		only_time,
		ms_time,
		custom,
	}stamp_t;

	Logging() = default;
	Logging(log_lvl_t l, const std::string &n): log_lvl(l), mod_name(n) {}

	struct settings{
		settings() = default;
		settings(log_lvl_t l, const std::string &n = LOGGER_NAME): lvl(l), module_name(n) {}

		log_lvl_t lvl = LOG_LVL_DEFAULT;
		std::string file_name = "";
		std::string module_name = LOGGER_NAME;
		uint32_t files_num = LOG_FILE_MAX_NUM;
		uint64_t file_size = LOG_FILE_MAX_SIZE; 
	};

	// Установка текущего уровня логирования, параметров лог файла
	void init(log_lvl_t lvl, 
		const std::string &module_name = LOGGER_NAME,
		const std::string &file_name = "", 
		uint32_t files_num = LOG_FILE_MAX_NUM,
		uint64_t file_size = LOG_FILE_MAX_SIZE
		)
	{
		set_lvl(lvl);
		mod_name = module_name;
		log_fname = file_name;
		log_max_fsize = file_size;
		max_files_num = files_num;
	}

	void init(log_lvl_t lvl, 
		const std::string &file_name = "", 
		uint32_t files_num = LOG_FILE_MAX_NUM,
		uint64_t file_size = LOG_FILE_MAX_SIZE
		)
	{
		set_lvl(lvl);
		log_fname = file_name;
		log_max_fsize = file_size;
		max_files_num = files_num;
	}

	void init(const settings &s){
		set_lvl(s.lvl);
		mod_name = s.module_name;
		log_fname = s.file_name;
		log_max_fsize = s.file_size;
		max_files_num = s.files_num;
	}

	// Получение текущего уровня логирования
	log_lvl_t get_lvl() const{
		std::lock_guard<std::mutex> lock(log_lvl_mutex);
		return log_lvl;
	}

	// Установка нового уровня логирования
	void set_lvl(log_lvl_t new_lvl){
		std::lock_guard<std::mutex> lock(log_lvl_mutex);
		log_lvl = new_lvl;
	}

	// Проверка необходимости подготовки сообщения для вывода
	bool check_lvl(log_lvl_t flags){
		log_lvl_t curr_lvl = get_lvl();
		log_lvl_t msg_lvl = flags & LOG_LVL_BIT_MASK;
		
		if(!(flags & MSG_TO_FILE) && (curr_lvl < msg_lvl)) return false;

		return true;
	}

	// Получение текущего формата временного штампа сообщения
	inline stamp_t get_time_stamp() const { return stamp_type; }
	inline const char* get_time_stamp_fmt() const { return stamp_fmt; }

	// Установка формата временного штампа сообщения ( fmt поддерживает формат strftime() )
	void set_time_stamp(stamp_t type) { stamp_type = type; }
	void set_time_stamp(const char *fmt) { stamp_fmt = fmt; stamp_type = custom; }

	// Установка имени модуля логирования
	void set_module_name(const std::string &new_name) { mod_name = new_name; }

	// Запись сообщения в лог-файл
	template<typename... Args>
	int to_file(const char *stamp, const char *fmt, Args&&... args) const;

	// Шаблонная версия форматированного вывода с переменным кол-вом параметров
	template<typename... Args>
	int msg(log_lvl_t flags, const char *fmt, Args&&... args) const;

	// Перегрузка для поддержки неформатированного вывода без предупреждений компилятора
	int msg(log_lvl_t flags, const std::string &str) const {
		return msg(flags, "%s", str);
	}

	int msg(log_lvl_t flags, const char *str) const {
		return msg(flags, "%s", str);
	}

	// Дамп блока памяти в 16-ричном формате
	void hex_dump(log_lvl_t flags, const uint8_t *buf, size_t len, const char *msg = "");

	// Формирование штампа сообщения
	static std::string make_msg_stamp(stamp_t type, const char *module_name, const char *fmt = "");

	// Мьютекс для целостного вывода комбинированных сообщений в stdout
	static std::recursive_mutex log_print_mutex; 
	// Мьютекс доступа к лог-файлу
	static std::recursive_timed_mutex log_file_mutex; 

	log_file_rotate_cb log_rotate = nullptr;	// колбек переполнения максимального размера лог-файта
	void *log_rotate_arg = nullptr;				// параметр  колбек ф-ии переполнения лог-файла

private:
	log_lvl_t log_lvl = LOG_LVL_DEFAULT;		// текущий уровень логирования
	std::string mod_name = LOGGER_NAME;			// имя экземпляра логера
	std::string log_fname = "";					// имя лог-файла
	uint64_t log_max_fsize = LOG_FILE_MAX_SIZE;	// максимальный допустимый размер лог-файла [Байт]
	uint32_t max_files_num = LOG_FILE_MAX_NUM;	// максимальное число лог файлов после ротации

	const char *stamp_fmt = "[ %d.%m.%y %T ]";	// формат вывода времени в штампе сообщения
	stamp_t stamp_type = Logging::date_time;	// тип формата вывода времени в штампе сообщения
	mutable std::mutex log_lvl_mutex;			// мьютекс доступа к текущему уровню логирования
	mutable uint32_t curr_file_num = 1;			// текущее число лог файлов

	// Получение текущего размера лог-файла
	static uint64_t get_file_size (const std::string &fname);
};


template<typename... Args>
int Logging::to_file(const char *stamp, const char *fmt, Args&&... args) const
{
	if( log_fname == "" || !log_max_fsize ) return 0;

	int ret = 0;
	std::FILE *logfp = nullptr;
	bool rotated = false;

	// Синхронизировать доступ к файлу с таймаутом 10 мс (файл может быть недоступен)
	std::unique_lock<std::recursive_timed_mutex> lock(log_file_mutex, std::defer_lock);
	if(!lock.try_lock_for(std::chrono::milliseconds(LOG_FILE_LOCK_MS))) return 0;

	uint64_t file_size = Logging::get_file_size(log_fname);

	// Если ошибка определения размера файла или максимальный размер превышен - перезапись файла
    if ( file_size >= log_max_fsize ){
        msg(MSG_VERBOSE, "------ Rotating log file ------\n");
        if(log_rotate) log_rotate(log_rotate_arg);

        // Процедура создания бэкапа лог-файла
        if(max_files_num){
        	std::string backup_name = log_fname + "." + std::to_string(curr_file_num);
        	std::rename(log_fname.c_str(), backup_name.c_str());
        	curr_file_num = (curr_file_num >= max_files_num) ? 1 : curr_file_num + 1;
        }

        logfp = fopen(log_fname.c_str(), "w+");
        rotated = true;
    }
    else logfp = std::fopen(log_fname.c_str(), "a+");

    if(logfp){
    	if(rotated) std::fprintf(logfp, "%s ----- Log file has been rotated -----\n", stamp);
    	if(stamp) std::fprintf(logfp, "%s ", stamp);
        ret = std::fprintf(logfp, fmt, to_c(args)...); 
        std::fclose(logfp);
        
    }
   	else {
   		// log_perr("Couldnt open log file '%s'", log_fname); 
   	}  

   	return ret;
}

template<typename... Args>
int Logging::msg(log_lvl_t flags, const char *fmt, Args&&... args) const
{
	// Если не предоставлено имя модуля
	// if(mod_name == "")) return 0;

	int ret = 0;
	std::string msg_stamp;
	
	log_lvl_t curr_lvl = this->get_lvl();
	log_lvl_t msg_lvl = flags & LOG_LVL_BIT_MASK;
	// Проверка необходимости подготовки сообщения для вывода
	if(!(flags & MSG_TO_FILE) && (curr_lvl < msg_lvl)) return 0;

	// Синхронизация формирования сообщения и вывода в stdout
	{
		std::unique_lock<std::recursive_mutex> lock(log_print_mutex);

		// Создание форматированной метаинформации о сообщении
		msg_stamp = Logging::make_msg_stamp(stamp_type, mod_name.c_str(), stamp_fmt);

		// Проверка уровня сообщения для вывода в терминал
		// (игнорируем сообщения только для записи в файл и с уровнем выше заданного допустимого)
		if(msg_lvl && msg_lvl <= curr_lvl){
			std::printf("%s ", msg_stamp.c_str());
			ret = std::printf(fmt, to_c(args)...);
		}
	}
	
	// Проверка необходимости записи сообщения в файл
	if(flags & MSG_TO_FILE){
		ret = this->to_file(msg_stamp.c_str(), fmt, args...);
	}

    return ret;
}

template <typename T>
const char* fmt_of(T arg)
{
    std::string arg_type = typeid(arg).name();

    // log_msg(MSG_OVERTRACE, "%s arg_type: %s\n", __func__, arg_type);

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

inline std::string method_name(const std::string &pretty_function)
{
    size_t colons = pretty_function.find("::");
    size_t begin = pretty_function.substr(0, colons).rfind(" ") + 1;
    size_t end = pretty_function.find("(") - begin;

    return pretty_function.substr(begin, end) + "()";
}

#define __METHOD_NAME__ method_name(__PRETTY_FUNCTION__)

// Функциональный макрос для формирования сообщения в месте возниковения исключения функции
#define excp_func(str) ( (std::string)__func__ + "():" + std::to_string(__LINE__) + " " + (str) )

// Функциональный макрос для формирования сообщения в месте возниковения исключения метода класса
#define excp_method(str) ( __METHOD_NAME__ + ": " + (str) )

#define _log_excp(obj, str...) do{					\
	Logging::stamp_t tmp = (obj).get_time_stamp(); 	\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, _RED "EX:" _RESET); \
	(obj).set_time_stamp(Logging::no_stamp); 		\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, str); 		\
	(obj).set_time_stamp(tmp); 						\
}while(0)


#define _log_info(obj, str...) do{					\
	Logging::stamp_t tmp = (obj).get_time_stamp(); 	\
	(obj).msg(MSG_INFO | MSG_TO_FILE, _YELLOW "INFO:" _RESET); \
	(obj).set_time_stamp(Logging::no_stamp); 		\
	(obj).msg(MSG_INFO | MSG_TO_FILE, str); 		\
	(obj).set_time_stamp(tmp); 						\
}while(0)

//
#define _log_msg_ns(obj, flags, str...) do{ 		\
	Logging::stamp_t tmp = (obj).get_time_stamp(); 	\
	(obj).set_time_stamp(Logging::no_stamp); 		\
	(obj).msg(flags, str); 							\
	(obj).set_time_stamp(tmp); 						\
}while(0)

// Приватная реализация макроса для логированя критических ошибок с подсветкой 
#define _log_err(obj, str...)	do{ 				\
	Logging::stamp_t tmp = (obj).get_time_stamp(); 	\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, _RED _BOLD "ERR:" _BOLD "%s %s():%d " _RESET, __FILE__, __func__, __LINE__ ); \
	(obj).set_time_stamp(Logging::no_stamp); 		\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, str); 		\
	(obj).set_time_stamp(tmp); 						\
}while(0)

// Приватная реализация макроса для логированя системных ошибок с подсветкой описания
#define _log_perr(obj, str...) do{ 					\
	Logging::stamp_t tmp = (obj).get_time_stamp(); 	\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, _RED _BOLD "PERR:" _BOLD "%s %s():%d " _RESET, __FILE__, __func__, __LINE__); \
	(obj).set_time_stamp(Logging::no_stamp); 		\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, str); 		\
	(obj).msg(MSG_ERROR | MSG_TO_FILE, ":%s\n", strerror(errno)); \
	(obj).set_time_stamp(tmp); 						\
}while(0)

// Приватная реализация макроса для логированя предупреждений
#define _log_warn(obj, str...)	do{ 				\
	Logging::stamp_t tmp = (obj).get_time_stamp(); 	\
	(obj).msg(MSG_WARNING | MSG_TO_FILE, _YELLOW _BOLD "WARN:" _RESET); \
	(obj).set_time_stamp(Logging::no_stamp); 		\
	(obj).msg(MSG_WARNING| MSG_TO_FILE, str); 		\
	(obj).set_time_stamp(tmp); 						\
}while(0)

// Определение макросов в зависимости от формата работы: 
// один логер на несколько модулей или
// у каждого модуля свой собственный экземпляр
#ifdef _SHARED_LOG
extern Logging logger;

// Название модуля при подключении логера для штампа сообщений
#ifdef LOG_MODULE_NAME
	#define MODULE_NAME 	LOG_MODULE_NAME	
#else
	#define MODULE_NAME 	""
#endif

// Функциональный макрос вывода отладочного сообщения с меткой времени и даты
#define log_msg(flags, str...)	do{ 				\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	logger.set_module_name(MODULE_NAME); 			\
	logger.msg(flags, str); 						\
}while(0)

// Вывод сообщения без штампа
#define log_msg_ns(flags, str...) do{ 				\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	_log_msg_ns(logger, flags, str); 				\
}while(0)

// Функциональный макрос вывода предупреждающих сообщений
#define log_warn(str...)	do{ 					\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	logger.set_module_name(MODULE_NAME); 			\
	_log_warn(logger, str); 						\
}while(0)

//
#define log_info(str...)	do{ 					\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	logger.set_module_name(MODULE_NAME); 			\
	_log_info(logger, str); 						\
}while(0)

// Функциональный макрос вывода сообщения об исключении
#define log_excp(str...) do{						\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	logger.set_module_name(MODULE_NAME); 			\
	_log_excp(logger, str);							\
}while(0)

// Функциональный макрос для логированя с подсветкой критических ошибок
#define log_err(str...)	do{ 						\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	logger.set_module_name(MODULE_NAME); 			\
	_log_err(logger, str); 							\
}while(0)

// Функциональный макрос для логированя системных ошибок с подсветкой описания
#define log_perr(str...) do{ 						\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	logger.set_module_name(MODULE_NAME); 			\
	_log_perr(logger, str); 						\
}while(0)

#else

#define log_msg(obj, flags, str...)		obj.msg(flags, str)

#define log_warn(obj, str...)	do{ 				\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	_log_warn(obj, str); 							\
}

// Функциональный макрос для логированя с подсветкой критических ошибок
#define log_err(obj, str...)	do{ 				\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	_log_err(obj, str); 							\
}while(0)

// Функциональный макрос для логированя системных ошибок с подсветкой описания
#define log_perr(obj, str...) do{ 					\
	std::unique_lock<std::recursive_mutex> lock(Logging::log_print_mutex); \
	_log_perr(obj, str); 							\
}while(0)

#endif


#endif
