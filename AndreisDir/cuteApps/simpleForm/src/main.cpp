 #include <QtGui>
 #include "simpleForm.h"

 int main(int argc, char *argv[])
 {
     QApplication app(argc, argv);

     SimpleForm simpleForm;
     simpleForm.show();
     
     return app.exec();
}
