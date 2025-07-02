В данном репозитории представлена реализация Dbus сервиса на языке C++, написанная с использованием библиотеки sdbus-c++.

Файл server.cpp содержит код для сервиса именем com.system.configurationManager на сессионной шине , который:
  1. При запуске считывает файлы конфигураций приложений из папки ~/
com.system.configurationManager/ и создает для каждого файла конфигурации D-Bus объект  
/com/system/configurationManager/Application/{applicationName} с интерфейсом com.system. 
configurationManager.Application.Configuration¨
  2. Предоставляет интерфейс com.system.configurationManager.Application.Configuration с методами:
    void ChangeConfiguration(key: string, value: variant), который изменяет определенный параметр для приложения и возвращает D-Bus ошибку в случае ошибки;
    map<string, variant> GetConfiguration(), который возвращает полную конфигурацию приложения;
  3. Предоставляет интерфейс com.system.configurationManager.Application.Configuration с сигналом: configurationChanged(configuration: dict), где dict является D-Bus типом a{sv};
 
Файл application.cpp содержит код для приложения confManagerApplication1 на языке С++, которое
  1. Подписывается на сигнал com.system.configurationManager.Application.configurationChanged у объекта /com/system/configurationManager/Application/confManagerApplication1 на сервисе 
     com.system.configurationManager и применяет новые параметры в случае их изменения;
  2. имеет управляемую конфигурацию ~/con.system.configurationManager/confManagerApplication1.json (файл можно положить в папку руками или при установке приложения), в которой есть два параметра:  
    1) "Timeout": uint - время (мс), через которое в stdout будет выведена какая-то строка, 
      определяемая вторым параметром;  
    2) "TimeoutPhrase": string - строка, которая будет выведена через "Timeout" мс.

Для сборки:
  mkdir build
  cd build
  cmake ..
  make
