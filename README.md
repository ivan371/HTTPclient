HTTP-клиент. 

Принцип работы: получаем адрес, начинающийся с http (http://example.com)
Программа парсит адрес на хост, путь к файлу и на имя файла (если в пути указана html-страница, то файл по умолчанию index.html)
Потом по имени хоста узнается ip-адрес сайта и мы создаем сокет, через который подключаемся к хосту. Потом отпавляем HEAD запрос на данный хост. После этого программа смотрит на статус ответа и делает GET запрос на получение файла только при 2**-м статусе.
Получая HEAD запрос, мы вычисляем его длину и начинаем запись файла пропуская заголовочную информацию. 
