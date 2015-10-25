#include "port.h"

void Port::send(const Message_t& message) {
    messages.append(message);
}

bool Port::read(Message_t* message) {
    if(empty())
        return false;
    *message = messages.pop();
    return true;
}

bool Port::empty() {
    return messages.size() == 0;
}

