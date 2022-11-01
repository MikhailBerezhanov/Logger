# Логирование сообщений для C++ 

### Пример использования

```C
#include <string>
#include <stdexcept>

#include "logger.hpp"

int main(int argc, char* argv[])
{
    Logging logger(MSG_DEBUG);

    const char *text = "const char* message";
    logger.msg(MSG_DEBUG | MSG_TO_FILE, "Message to stdout: %s\n", text);

    std::string str = "std::srtring message";
    logger.msg(MSG_DEBUG | MSG_TO_FILE, "Message to stdout AND to file: %s\n", str);

    logger.msg(MSG_ERROR, "error messsage");

    try{
        // 
        throw std::runtime_error(excp_func("runtime exception"));
    }
    catch(const std::exception &e){
        logging_excp(logger, e.what());
    }

    return 0;
}
```

### Инициализация и создание объекта

Логер имеет несколько параметров инициализации:

* `уровень логирования` (по умолчанию MSG_ERROR)
* `название логера` (по умолчанию не задано - пустая строка)
* `абсолютный путь к лог-файлу` (по умолчанию не задан - вывод только в stdout)
* `максимально допустимое число ротаций` - кол-во хранимых файлов лога (по умолчанию 3)
* `максимально-допустимый размер лог-файла в байтах`. Если установлен в 0, перезаписывание и очистка файла не производится. Для удобства определения можно использовать предоставленные 
макросы `MB_to_B(x)` , `KB_to_B(x)` осуществляющие перевод Мбайт и Кбайт в Байты соответсвенно. По умолчанию 2 МБ. 

```C
// Создание логера с уровнем логирования и именем, но без вывода в файл
Logging logger(MSG_VERBOSE, "StdoutLogger");

// Создание логера с выводом только в файл (до 3 файлов максимального размера 2 МБ)
Logging logger2(MSG_SILENT, "FileLogger", "Log.log", 3, MB_to_B(2));

// Создание логера с выводом сообщений уровня DEBUG и ниже и записью в файл
Logging logger3(MSG_DEBUG, "Stdout_File_Logger", "Log.log");

Logging default_logger;
// Для логера по умолчанию допступны сообщения только уровня MSG_ERROR
default_logger.msg(MSG_ERROR, "some error text\n");
default_logger.msg(MSG_DEBUG, "some debug text\n"); // выведено не будет

// При необходимости после создания объекта, его можно переконфигурировать
default_logger.init(MSG_TRACE, "MyLog.log", 3, KB_to_B(2));
default_logger.msg(MSG_DEBUG | MSG_TO_FILE, "some debug text\n"); // теперь будет выведено и в stdout и в файл
default_logger.set_level(MSG_WARNING);

```

### Установка уровня логирования

Поддерживаемые уровни располагаются в порядке возрастания подробности сообщений:

0. `MSG_SILENT` - весь вывод запрещен

1. `MSG_ERROR` - вывод только ошибок

2. `MSG_WARNING` - вывод ошибок и предупреждений

3. `MSG_INFO` - разрешен вывод информационных сообщений

4. `MSG_DEBUG` - разрешен вывод отладочных сообщений

5. `MSG_VERBOSE` - разрешен вывод подробных сообщений

6. `MSG_TRACE` - разрешен вывод трассировачных сообщений

Иерархия уровней:  

`MSG_SILENT` < `MSG_ERROR` < `MSG_WARNING` < `MSG_INFO` < `MSG_DEBUG` < `MSG_VERBOSE` < `MSG_TRACE`

Кроме того есть дополнительный флаг разрешения записи сообщения в лог файл, допускающий сложение с вышеперечисленными уровнями 

* `MSG_TO_FILE`

Если файл не задан, данный флаг будет проигнорирован.

```C
Logging logger(MSG_DEBUG);
logger.msg(MSG_DEBUG | MSG_TO_FILE, "Message to stdout AND to file: %s\n", text);
```

### Особенности компиляции

Данный модуль имеет одно настроечное макро-определение `_SHARED_LOG` , которое (если определено) позволяет использовать один глобальный объект логера для всех файлов проекта. Кроме
того, становятся доступны вспомогательные макросы формирования сообщений не требующие указания объекта логера. 

```sh
# Компиляция с использованием общего объекта логирования
g++ main.cpp logger.cpp -D_SHARED_LOG -lpthread
# Обычная компиляция
g++ main.cpp logger.cpp -lpthread
```

При использовании опции `_SHARED_LOG` для каждого файла приложения становится доступно макроопределение `LOG_MODULE_NAME` для индикации источника сообщения. Иначе будет использовано имя логера переданное в конструкторе при создании объекта. 

```C
#define LOG_MODULE_NAME     "[ APP ]"
#include "logger.hpp"
```

Указанное название будет включено в штамп сообщения вызванного в соотвествующем файле приложения.

### Вспомогательные макросы

Для наиболее часто используемых сообщений предлагается использование следующих макросов
(некоторые из них подствечивают цветом уровень сообщения, источник исключений или текст ошибки):

* `logging_msg_ns(obj, flags, str...)` - Вывод сообщения без штампа
* `logging_warn(obj, str...)` - Вывод предупреждающих сообщений (MSG_WARNING | MSG_TO_FILE)
* `logging_info(obj, str...)` - Вывод информационных сообщений (MSG_INFO | MSG_TO_FILE)
* `logging_excp(obj, str...)` - Вывод сообщения об исключении
* `logging_err(obj, str...)` - Логирование критических ошибок с подсветкой описания (MSG_ERROR | MSG_TO_FILE)
* `logging_perr(obj, str...)` - Логирование системных ошибок с подсветкой описания (MSG_ERROR | MSG_TO_FILE)
* `logging_hexdump(obj, flags, buf, len, msg)` - Вывод дампа массива байт

Аналогичные макросы для режима `_SHARED_LOG` не требуют указания объекта:  

* `log_msg(flags, str...)`  
* `log_msg_ns(flags, str...)` 
* `log_warn(str...)`
* `log_info(str...)`
* `log_excp(str...)`
* `log_err(str...)`
* `log_perr(str...)`
* `log_hexdump(flags, buf, len, msg)`

Для формирования информации об источнике исключения предоставляются макросы:

* `excp_func(str)` - сообщение str оборачивается в информацию о функции, в которой сгенерировано
* `excp_method(str)` - сообщение str оборачивается в информацию о методе класса, в котором сгенерировано

### Формат штампа сообщений

Для задания формата преамбулы сообщений существует два метода:

* `set_time_stamp(stamp_t type)` - установка формата из перечисения __Logging::stamp_t__
* `set_time_stamp(const char *fmt)` - установка пользовательского формата. Должен соответствовать поддерживаемым ключам [strftime](https://man7.org/linux/man-pages/man3/strftime.3.html) 
 

Формат по умолчанию: __Logging::stamp_t::date_time__ - `"[ %d.%m.%y %T ]"`
