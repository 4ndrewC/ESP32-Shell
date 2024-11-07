#include <iostream>
#include <fstream>

#include <string>
#include <bits/stdc++.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

using namespace std;

#define INITIAL_VERT 12
#define VERT_INCREMENT 30
#define NEW_COMMAND "> "
#define NEW_MESSAGE "    "

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 900
#define TEXTBOX_WIDTH 400
#define TEXTBOX_HEIGHT 30
#define CURSOR_WIDTH 2
#define CURSOR_FLASH_INTERVAL 500


int y = INITIAL_VERT;
vector<string> history;
vector<int> message_type; // 1 means message, 0 means command
int newline;

/* commands woooo */

void print_serial_log(){
    ifstream serial("serial log.txt");
    if(!serial.is_open()) cerr<<"uh oh stinky"<<endl;
    string line;
    vector<string> temp;
    while(getline(serial, line)){
        temp.push_back(line);
    }
    /* do command specific modification to the vector */

    // for now just print all
    for(int i = 0; i<temp.size(); i++){
        history.push_back(temp[i]);
        message_type.push_back(1);
    }
}

/* ----- commands end --------*/

void renderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y){

    SDL_Color textColor = {255, 255, 255}; // white text for now
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
    if(textSurface == nullptr){
        std::cerr << "Unable to render text surface! SDL_ttf Error: " << TTF_GetError() << std::endl;
        return;
    }

    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if(textTexture == nullptr){
        std::cerr << "Unable to create texture from rendered text! SDL Error: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(textSurface);
        return;
    }

    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);

    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}

int main(int argc, char* argv[]){
    if(SDL_Init(SDL_INIT_VIDEO) < 0){
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    if(TTF_Init() == -1){
        std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("ESP-32 Shell",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH,
                                          WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if(window == nullptr){
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("fonts/mono/CascadiaMono.ttf", 24);
    if (font == nullptr) {
        std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    string inputText;
    bool running = true;
    SDL_Event event;

    SDL_StartTextInput();

    bool cursorVisible = true;
    Uint32 lastCursorToggleTime = SDL_GetTicks();

    while (running) {
        newline = 0;
        string current_line;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_TEXTINPUT) {
                inputText += event.text.text;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty()) {
                    inputText.pop_back(); // strip right space
                    // might need to strip left space too
                }
            } else if(event.key.keysym.sym==SDLK_RETURN || event.key.keysym.sym==SDLK_KP_ENTER){
                std::cout<<"entered!"<<std::endl;
                y+=VERT_INCREMENT;
                history.push_back(inputText);
                current_line = inputText;
                message_type.push_back(0);
                inputText = "";
                newline = 1;
            }
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // black background
        SDL_RenderClear(renderer);

        /* ---------- -render text code ------------ */
        
        int temp_index = 0;
        if(history.size()>0){
            for(int i = 0; i<history.size(); i++){
                if(!message_type[i]) renderText(renderer, font, NEW_COMMAND+history[i], 12, INITIAL_VERT+i*VERT_INCREMENT);
                else renderText(renderer, font, NEW_MESSAGE+history[i], 12, INITIAL_VERT+i*VERT_INCREMENT);
                temp_index++;
            }
        }

        /* command parse */
        if(current_line=="port logs -a") print_serial_log();
        if(history.size()>0){
            for(int i = temp_index; i<history.size(); i++){
                if(!message_type[i]) renderText(renderer, font, NEW_COMMAND+history[i], 12, INITIAL_VERT+i*VERT_INCREMENT);
                else renderText(renderer, font, NEW_MESSAGE+history[i], 12, INITIAL_VERT+i*VERT_INCREMENT);
                y+=VERT_INCREMENT;
            }
        }

        renderText(renderer, font, NEW_COMMAND+inputText, 12, y);

        
        /* ----------- render text end ------------- */
    
        
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastCursorToggleTime >= CURSOR_FLASH_INTERVAL) {
            cursorVisible = !cursorVisible;
            lastCursorToggleTime = currentTime;
        }

        if (cursorVisible) {
            int cursorX = 15 + ((NEW_COMMAND+inputText).size() * 14);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect cursorRect = {cursorX, y, CURSOR_WIDTH, TEXTBOX_HEIGHT};
            SDL_RenderFillRect(renderer, &cursorRect);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
