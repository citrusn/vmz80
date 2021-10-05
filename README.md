# vmz80
Эмулятор спектрума
Переделан на SDL2 и откомпилирован под Windows. 
Изменена работа со звуком, иначе давал наводку 50 Гц в динамики :))
# Параметры командной строки

```
-2 Включить режим 128к
-a Автостарт с командой RUN
-b <file> <offsethex> Загрузка любого бинарного файла в память
-c Запускать без GUI SDL
-d Включить отладчик при загрузке
-h Останавливать выполнение на halt для консольного режима
-k "последовательность символов нажатий клавиш" (под вопросом)
-m <кадры> Пропуск кадров
-M <секунды> длительность записи
-o <файл> Вывод серии PNG в файл (если - то stdout)
-p <address> Установка адреса PC после запуска
-r<0,1,4> <rom-файл> Загрузка ROM 0:128k, 1:48k, 4:TrDOS (под вопросом, загружаются сами, не понятно как выбрать)
-s Пропуск повторяющегося кадра
-w wav-файл для записи звука
-x Отключить звук
-z включение моно-звука
<file>.(z80|tap|sna) Загрузка снашпота или TAP бейсика
```
