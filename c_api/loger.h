#ifndef _LOGER_H
#define _LOGER_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

// Глобальная настройка приложения: разрешение синхронизации вывода
#if APP_PTHREADED
	#include <pthread.h>
	#define LOG_MUTUAL	1
	extern pthread_mutex_t log_lvl_mutex;
	extern pthread_mutex_t log_file_mutex;
	extern pthread_mutex_t log_print_mutex;
	#define	log_lvl_lock() 		pthread_mutex_lock(&log_lvl_mutex);
	#define log_lvl_unlock() 	pthread_mutex_unlock(&log_lvl_mutex);
	// Файл может быть недоступен, чтобы потоки не зависали в ожидании - таймаут на мьютекс 10 мс
	#define	log_file_lock()		do{												\
		struct timespec abs_timeout; 											\
	    if(clock_gettime(CLOCK_REALTIME, &abs_timeout) < 0) return; 			\
	    abs_timeout.tv_nsec += 10*1000000; 										\
		if(pthread_mutex_timedlock(&log_file_mutex, &abs_timeout)) return; 		\
	}while(0)
	#define log_file_unlock()	pthread_mutex_unlock(&log_file_mutex);
	#define	log_print_lock() 	pthread_mutex_lock(&log_print_mutex);
	#define log_print_unlock() 	pthread_mutex_unlock(&log_print_mutex);
#else 
	#define LOG_MUTUAL	0
	#define	log_lvl_lock()
	#define log_lvl_unlock()
	#define log_file_lock()
	#define log_file_unlock()
	#define	log_print_lock()
	#define log_print_unlock()
#endif

// Настройка вывода для отдельных модулей (если не определена - логирование не производится)
#ifdef LOG_MODULE_NAME
	#define MODULE_NAME 		LOG_MODULE_NAME
#else
	#define MODULE_NAME 		""
#endif

// Функция вывода лога в терминал
#define log_print(str...) 		printf(str)

// Уровень сообщения для лога
typedef uint8_t log_lvl_t;

// Функция, вызываемая при достижении лог файлом максимального размера (перед ротацией) из log_to_file().
// ПРИМ: Внутри колбек функции НЕЛЬЗЯ использовать запись лога в файл (флаг MSG_TO_FILE и log_to_file())
//		 во избежание рекурсии вызовов.
typedef void (*log_rotate_cb)(void);


// Флаги - уровни логирования сообщения (активные 7бит)
#define MSG_SILENT				( (log_lvl_t)(0) )
#define MSG_ERROR				( (log_lvl_t)(1) )
#define MSG_DEBUG				( (log_lvl_t)(2) )
#define MSG_VERBOSE				( (log_lvl_t)(3) )
#define MSG_TRACE				( (log_lvl_t)(4) )
#define MSG_OVERTRACE			( (log_lvl_t)(5) )
// Флаг логирования сообщения в файл
#define MSG_TO_FILE				( (log_lvl_t)(1 << 7) )
// Битовая маска выделения уровня сообщения (7бит)
#define LOG_LVL_BIT_MASK		( (log_lvl_t)(0x7F) )


/**
  * @описание   Инициализация логера
  * @параметры
  *     Входные:
  *         fname - имя лог-файла или NULL
  *         max_fsize - максимально допустимый размер лог-файла (или 0 для отключения ротации)
  *         cb - функция, вызываемая при ротации лог-файла
  * @возвращает Строку с результатом выполнения команды
 */
void log_init(const char* fname, uint64_t max_fsize, log_rotate_cb cb);

// Типы поддерживаемых форматов штампа сообщения
typedef enum {
	no_stamp = 0,
	dtime_stamp,
	time_stamp,
	msec_stamp
}stamp_t;

/**
  * @описание   Создание штампа для сообщения: [ дата время ] имя_модуля
  * @параметры
  *     Входные:
  *			type - тип формата времени
  *         module_name - имя модуля
  *         buf_len - размер буфера для штампа
  *     Выходные:
  *         buf - буфер, в который записывается сгенерированный штам
 */
void log_make_stamp (stamp_t type, const char* module_name, char* buf, size_t buf_len);

/**
  * @описание   Установка уровня логирования
  * @параметры
  *     Входные:
  *         new_lvl - новый уровень логирования
 */
void log_set_level(log_lvl_t new_lvl);

/**
  * @описание   Получение текущего уровня логирования
  * @возвращает Текущий уровень логирования
 */
log_lvl_t log_get_level(void);

/**
  * @описание   Проверка необходимости логирования сообщения
  * @параметры
  *     Входные:
  *         flags - флаги уровня сообщения
  * @возвращает true если сообщение надо логировать, иначе - false
 */
bool log_check_level(log_lvl_t flags);

/**
  * @описание   Запись сообщения в лог-файл
  * @параметры
  *     Входные:
  *         stamp   - штамп сообщения
  *         format  - форматированное сообщение
 */
void log_to_file(const char* stamp, const char* format, ...);

/**
  * @описание   Вывод массива байт
  * @параметры
  *     Входные:
  *         flags   - флаги уровня сообщения
  *         msg     - предварительное сообщение перед выводом массива
  *         buf     - указатель на массив для вывода
  *         len     - размер массива
 */
void log_print_arr(log_lvl_t flags, const char* msg, uint8_t* buf, size_t len);

void log_hexdump (log_lvl_t flags, const void *_dump, size_t len, size_t offset);
void log_hexstr (log_lvl_t flags, const void *_dump, size_t len);

// Коды Цветов для терминала
#define _RED     	"\x1b[31m"
#define _GREEN   	"\x1b[32m"
#define _YELLOW  	"\x1b[33m"
#define _BLUE    	"\x1b[34m"
#define _MAGENTA 	"\x1b[35m"
#define _CYAN    	"\x1b[36m"
#define _LGRAY   	"\x1b[90m"
#define _RESET   	"\x1b[0m"
#define _BOLD 	 	"\x1b[1m"

// Функциональные макросы
// Расширенная Запись сообщения в стандартный вывод и файл, в зависимоти от уровня
// # stamp - признак наличия штампа в выводе (0 - запрещено, иначе - разрешено)
// # flags - флаги уровня сообщения (допускается сложение: MSG_DEBUG | MSG_TO_FILE)
// # str - форматированное сообщение (С-строка)
#define log_msg_ex(stamp, flags, str...) do{ 									\
	if(!strcmp(MODULE_NAME, "")) break; 										\
	if(!(flags)) break; 														\
	log_lvl_t _curr_lvl = log_get_level(); 										\
	log_lvl_t _msg_lvl = (flags) & LOG_LVL_BIT_MASK;							\
	if(!((flags) & MSG_TO_FILE) && (_curr_lvl < _msg_lvl)) break; 				\
	char msg_stamp[64] = {0}; 													\
	if(stamp) log_make_stamp(stamp, MODULE_NAME, msg_stamp, sizeof msg_stamp); 	\
	if(_msg_lvl && _msg_lvl <= _curr_lvl){ 										\
		log_print("%s", msg_stamp); 											\
		log_print(str); 														\
	} 																			\
	if((flags) & MSG_TO_FILE) log_to_file(msg_stamp, str); 						\
}while(0)

// Запись сообщения в стандартный вывод и файл, в зависимоти от уровня
#define log_msg(flags, str...) 	log_msg_ex(dtime_stamp, (flags), str)

// Вывод отладочного сообщения
#define log_dbg(str...)			log_msg_ex(dtime_stamp, MSG_DEBUG, str)


#define ERR_BUF_SIZE			256

// Вывод сообщения об ошибке
#define log_err_ex(flags, str...) do{ 											\
	if(!log_check_level(flags)) break;											\
	char buf[ERR_BUF_SIZE] = {0}; 												\
	snprintf(buf, sizeof buf, _RED "[ %s() error ]" _RESET " ", __func__); 		\
	size_t len = strlen(buf); 													\
	snprintf(&buf[len], sizeof(buf) - len, str); 								\
	log_msg_ex(dtime_stamp, (flags), "%s", buf); 								\
}while(0)

// Вывод и запись в файл сообщения об ошибке
#define log_err(str...) 		log_err_ex(MSG_ERROR | MSG_TO_FILE, str)

// Вывод сообщения об ошибке в стиле perr
#define log_perr_ex(flags, str...) do{ 											\
	if(!log_check_level(flags)) break;											\
	char buf[ERR_BUF_SIZE] = {0}; 												\
	snprintf(buf, sizeof buf, _RED "[ %s() perror ]" _RESET " ", __func__); 	\
	size_t len = strlen(buf); 													\
	snprintf(&buf[len], sizeof(buf) - len, str); 								\
	len = strlen(buf); 															\
	snprintf(&buf[len], sizeof(buf) - len, ": %s\n", strerror(errno)); 			\
	log_msg_ex(dtime_stamp, (flags), buf); 										\
}while(0)

// Вывод и запись в файл сообщения об ошибке в стиле perr
#define log_perr(str...) 		log_perr_ex(MSG_ERROR | MSG_TO_FILE, str)


#endif
