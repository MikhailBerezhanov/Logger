
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>

#define LOG_MODULE_NAME     "[ LOGGER ]"
#include "logger.h"

#define HEXLINE_MOD 16

static log_lvl_t curr_log_level = MSG_DEBUG; 
static char log_fname[640] = {0};
static uint64_t log_max_fsize = 0;   // По умолчанию не задан (ротация лог-файла запрещена)
static uint max_files_num = 3;
static uint curr_file_num = 1;
static log_rotate_cb log_cb = NULL;         

#if LOG_MUTUAL
pthread_mutex_t log_lvl_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_file_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_mutex_t log_print_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

// Определение размера файла
static uint64_t get_filesize (const char* fname)
{
    struct stat st;
    memset(&st, 0, sizeof(st));

    if(stat(fname, &st) < 0){
        log_perr_ex(MSG_ERROR, "\'%s\' stat failed", fname);
        return 0;
    }

    return (uint64_t)st.st_size;
}


// Инициализация логера
void log_init(const char* fname, uint64_t max_fsize, uint max_files, log_rotate_cb cb)
{
    if(fname) strcpy(log_fname, fname);
    else strcpy(log_fname, "");
    log_max_fsize = max_fsize;
    max_files_num = max_files;
    log_msg(MSG_DEBUG, "Setting log_max_fsize to: %" PRIu64 " [B]\n", log_max_fsize);
    log_cb = cb;
}

// Создание штампа для сообщения: [ дата время ] имя_модуля
void log_make_stamp (stamp_t type, const char* module_name, char* buf, size_t buf_len)
{
    char time_str[32] = {0};
	struct tm timeinfo;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

	if(!localtime_r(&spec.tv_sec, &timeinfo)){
		strncpy(time_str, "localtime_r failed", sizeof time_str);
		return;
	}
    
    switch(type){
        case time_stamp: strftime(time_str, sizeof time_str, "[ %T ]", &timeinfo); break;
        case msec_stamp: 
            strftime(time_str, sizeof time_str, "%d.%m.%y %T", &timeinfo); 
            snprintf(buf, buf_len, "[ %s.%03lu ] %s", time_str, spec.tv_nsec / 1000000L, module_name);
            return;

        case dtime_stamp: 
        default:
            strftime(time_str, sizeof time_str, "[ %d.%m.%y %T ]", &timeinfo); break;
    }
    snprintf(buf, buf_len, "%s %s", time_str, module_name);
}

// Установка уровня логирования
inline void log_set_level(log_lvl_t _new_lvl)
{
	log_lvl_lock();
	curr_log_level = _new_lvl;
	log_lvl_unlock();
}

// Получение текущего уровня логирования
inline log_lvl_t log_get_level(void)
{
	log_lvl_lock();
	log_lvl_t _ret = curr_log_level;
	log_lvl_unlock();
	return _ret;
}

// Проверка необходимости логирования сообщения
inline bool log_check_level(log_lvl_t _flags)
{
    log_lvl_t _curr_lvl = log_get_level();
    log_lvl_t _msg_lvl = _flags & LOG_LVL_BIT_MASK;

    // Если сообщение не для файла и не для текущего уровня логирования 
    if(!(_flags & MSG_TO_FILE) && (_curr_lvl < _msg_lvl)) return false;

    return true;
}

// Запись сообщения в лог-файл
void log_to_file(const char *stamp, const char *format, ...)
{
    if( !strcmp(log_fname, "") || !log_max_fsize ) return;

    FILE* logfp = NULL;
    char str[1024] = {0};
    
    va_list args;
    va_start(args, format);
    vsnprintf(str, sizeof(str), format, args);

    log_file_lock();

    uint64_t log_size = get_filesize(log_fname);
    //log_print("log_size: %" PRIu64 ", log_max_fsize: %" PRIu64 "\n", log_size, log_max_fsize);

    if ( log_size >= log_max_fsize ){
        log_msg(MSG_VERBOSE, "------ Rotating log file ------\n");
        // call rotation callback function
        if(log_cb) log_cb();

        // Процедура создания бэкапа лог-файла
        if(max_files_num){
            char *backup_name = NULL;
            if(asprintf(&backup_name, "%s.%u", log_fname, curr_file_num) > 0){
                rename(log_fname, backup_name);
                curr_file_num = (curr_file_num >= max_files_num) ? 1 : curr_file_num + 1;
                free(backup_name);
            }
        }

        logfp = fopen(log_fname, "w+");
        if(logfp) {
            if(stamp) fprintf(logfp, "%s", stamp);
            fprintf(logfp, "------ Log has been rotated ------\n");
        }
    }
    else {
        logfp = fopen(log_fname, "a+");
    }

    if(logfp){
    	if(stamp) fprintf(logfp, "%s", stamp);
        fprintf(logfp, "%s", str);
        fclose(logfp);
    }
    else log_dbg("Couldnt open log file '%s'\n", log_fname);   

    log_file_unlock();

    format = va_arg(args, char*);
    va_end(args);
}

// Вывод массива байт
void log_print_arr(log_lvl_t flags, const char* msg, uint8_t* buf, size_t len)
{
    if(!log_check_level(flags)) return;

    log_print_lock();

    log_msg_ex(dtime_stamp, flags, "%s", msg);
    for (int i = 0; i < len; i++){
        if (i != 0 && i % (HEXLINE_MOD / 2) == 0)
            log_msg_ex(no_stamp, flags, "  ");
        if (i != 0 && i % HEXLINE_MOD == 0)
            log_msg_ex(no_stamp, flags, "\n");
        log_msg_ex(no_stamp, flags, "%02X ", buf[i]);
    }
    log_msg_ex(no_stamp, flags, "\n");

    log_print_unlock();
}


static unsigned char toprint (const unsigned char chr) 
{
	// https://www.ascii-codes.com/cp855.html
	// Standard (32 - 126) and extended (128 - 254) character set :
	return ((chr > 0x1f && chr < 0x7f) || (chr >= 0x80 && chr < 0xFF)) ? chr : '.';
}

static void log_hexline (log_lvl_t flags, const unsigned char *dump, size_t len) 
{
	size_t i;
	char str[HEXLINE_MOD + 1] = {0};

	for (i = 0; i < len && i < HEXLINE_MOD; i++) {
		log_msg_ex(no_stamp, flags, "%02X ", dump[i]);
		str[i] = toprint (dump[i]);
		if (i == HEXLINE_MOD / 2 - 1)
			log_msg_ex(no_stamp, flags, " ");
	}
	for (; i < HEXLINE_MOD; i++) {
		if (i == HEXLINE_MOD / 2 - 1)
			log_msg_ex(no_stamp, flags, " ");
		log_msg_ex(no_stamp, flags, "   ");
	}
	log_msg_ex(no_stamp, flags, " |%s|", str);
}

void log_hexdump (log_lvl_t flags, const void *_dump, size_t len, size_t offset) 
{
    if(!log_check_level(flags)) return;

	const unsigned char *dump = (const unsigned char*) _dump;
	for (size_t i = 0; i < len; i += HEXLINE_MOD) {

		log_msg_ex(dtime_stamp, flags, "%08zx  ", offset + i);

		log_hexline(flags, &dump[i], len - i);
		log_msg_ex(no_stamp, flags, "\n");
	}
}

void log_hexstr (log_lvl_t flags, const void *_dump, size_t len) 
{
    if(!log_check_level(flags)) return;

	const unsigned char *dump = (unsigned char*) _dump;
	size_t i;
	for (i = 0; i < len; i++) {
		if (i != 0 && i % (HEXLINE_MOD / 2) == 0)
			log_msg_ex(no_stamp, flags, "  ");
		if (i != 0 && i % HEXLINE_MOD == 0)
			log_msg_ex(no_stamp, flags, "\n");
		log_msg_ex(no_stamp, flags, "%02X ", dump[i]);
	}
	log_msg_ex(no_stamp, flags, "\n");
}


#ifdef _LOGGER_TEST
int main(int argc, char* argv[])
{
    log_init("logger-c.log", DFLT_FILE_SIZE, 3, NULL);
    log_set_level(MSG_DEBUG);
    const char *text = "Hello logger";
    log_msg(MSG_DEBUG | MSG_TO_FILE, "Message to stdout AND to file: %s\n", text);
}
#endif