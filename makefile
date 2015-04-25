CFLAGS=	-D NO_ENC -D__GPS -D USE_MYSQL -I /usr/include/libxml2 -I /usr/include/mysql -L /usr/lib64/mysql 

SRC=	main.c	\
	3des.c	\
	session.c \
	xmlutil.c \
	ws_util.c \
	portal.c gomoclient.c jsmn.c

OBJ=	$(SRC:.c=.o)

all:	iris_mysql

iris_mysql: $(OBJ)
	gcc $(CFLAGS) $(OBJ) -lmysqlclient  -lcrypto -lpthread -lssl -lxml2 -lcurl -lz -o $@

.c.o:	$(SRC)
	gcc -c $(CFLAGS) $< -o $@

clean:
	-rm -f *.o

