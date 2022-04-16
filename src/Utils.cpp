#include "Utils.h"

String Utils::randomString(int len) {
    String acc = "";
    for (int i = 0; i < len; i++) {
        acc += randomChar();
    }
    return acc;
}

char Utils::randomChar() {
    byte randomValue = random(0, 26);
    char letter = randomValue + 'a';
    if (randomValue > 26)
        letter = (randomValue - 26) + '0';
    return letter;
}