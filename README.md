# Модуль логирования сообщений для C\C++ 


Пример подключения логера к модулю приложения с разрешением вывода в stdout:

`#define LOG_MODULE_NAME     "[ MODULE_NAME ] "`

`#include "loger.h"`

Если задать `LOG_MODULE_NAME ` пустой строкой вывод сообщений будет запрещен.

Для синхронизации обращений к лог-файлу и целостности вывода в многопоточном приложении на **pthread** следует при компиляции определить макросы `APP_PTHREADED` и `_GNU_SOURCE`

`gcc main.c loger.c -DAPP_PTHREADED=1 -D_GNU_SOURCE=1`

### Установка текущего уровня логирования

Поддерживаемые уровни располагаются в порядке возрастания подробности сообщений:

* `MSG_SILENT` - весь вывод запрещен

* `MSG_ERROR` - вывод только ошибок

* `MSG_DEBUG` - разрешен вывод отладочных сообщений

* `MSG_VERBOSE` - разрешен вывод подробных сообщений

* `MSG_TRACE` - разрешен вывод трассировачных сообщений

* `MSG_OVERTRACE` - разрешен вывод сверх трассировачных сообщений

Кроме того есть дополнительный флаг разрешения записи сообщения в лог файл, допускающий сложение с вышеперечисленными уровнями 

* `MSG_TO_FILE`

`log_msg(MSG_DEBUG | MSG_TO_FILE, "Message to stdout AND to file: %s\n", text);`


Для установки текущего уровня логирования используется:

`log_set_level(MSG_DEBUG);`

Таким образом разрешены все сообщения такого же или уровня ниже
(выводиться будут `MSG_ERROR` и `MSG_DEBUG`) .



### Инициализация лог-файла

Если требуется запись сообщений в файл, то необходима дополнительная инициализация. Требуется указать абсолютный путь к файлу, максимальный допустимый размер в байтах и функцию для ротации файла при достижении максимального размера. 

* По умолчанию ведение лог-файла не включено и вывод осуществляется только в stdout. 

* При достижении файлом заданного размера вызывается функция `cb`, затем файл автоматически. 

* При установке `max_fsize = 0`, перезаписывание и очистка файла не производится. 

`log_init(const char* fname, uint64_t max_fsize, log_rotate_cb cb)`


### Вывод сообщений
Данный модуль предоставляет набор макросов для ведения лога:

* log_msg(flags, str...) - вывод сообщения с заданным уровнем

* log_dbg(str...) - отладочное сообщение

* log_err(str...)  - сообщение об ошибке с записью в файл

* log_perr(str...) - сообщение об ошибке в стиле perr с записью в файл

В начале каждого сообщения автоматически проставляется временной штамп и имя модуля из макроопределения `LOG_MODULE_NAME `

Пример вывода:

`#define LOG_MODULE_NAME     "[ APP ] "`

`#include "loger.h"`

`log_dbg("stamp and module name\n");`

[ 04.03.21 13:13:12 ] [ APP ] stamp and module name

Для управления форматом временного штампа используeтся расширенныq макрос вывода:

`log_msg_ex(stamp, flags, str...)`

Набор поддерживаемых форматов определен в **stamp_t**