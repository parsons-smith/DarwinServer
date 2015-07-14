OBJ = o
NAME = CMSServer
ALL = $(NAME)
all:	$(ALL)

OBJS = CMSServer.$(OBJ) tinyxml/tinystr.$(OBJ) \
	tinyxml/tinyxml.$(OBJ) tinyxml/tinyxmlerror.$(OBJ) \
	tinyxml/tinyxmlparser.$(OBJ)

tinyxml/tinyxml.$(OBJ):	tinyxml/tinyxml.h
tinyxml/tinystr.$(OBJ): tinyxml/tinystr.h

CMSServer: $(OBJS)
	g++ -o $(NAME) $(OBJS)

clean:
	-rm -rf *.$(OBJ) $(ALL) tinyxml/*.$(OBJ)
