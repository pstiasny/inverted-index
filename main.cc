#include "inverted_index.h"


int main(int argc, char** argv) {
    DB db("log");
    Parser p;
    Interpreter i(db, cout);

    string input;
    while (cin.good()) {
        cout << "ii> ";
        getline(cin, input);
        if (input.empty())
            continue;
        //Printer pr;
        //pr.interpret(*p.parse(input));
        try {
            i.interpret(*p.parse(input));
        } catch (ParseError &e) {
            cout << "PARSE ERROR " << e.what() << endl;
        } catch (CommandError &e) {
            cout << "COMMAND ERROR " << e.what() << endl;
        }
    }

    return 0;
}
