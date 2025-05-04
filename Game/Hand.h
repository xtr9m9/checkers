#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// methods for hands
class Hand
{
public:
    // Конструктор принимает указатель на объект Board (игровое поле)
    Hand(Board* board) : board(board)
    {
    }

    // Метод для получения клетки, выбранной пользователем
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;  // Объект для обработки событий SDL
        Response resp = Response::OK; // Переменная для хранения ответа (по умолчанию OK)
        int x = -1, y = -1;  // Координаты клика мыши
        int xc = -1, yc = -1; // Координаты клетки на доске

        while (true) // Бесконечный цикл ожидания событий
        {
            if (SDL_PollEvent(&windowEvent)) // Проверяем, есть ли событие в очереди
            {
                switch (windowEvent.type) // Определяем тип события
                {
                case SDL_QUIT: // Если игрок закрыл окно, завершаем игру
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN: // Обработчик клика мыши
                    x = windowEvent.motion.x; // Получаем координаты клика
                    y = windowEvent.motion.y;

                    // Определяем клетку на доске по координатам
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Проверяем, была ли нажата кнопка "назад"
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Проверяем, была ли нажата кнопка "повтор"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Проверяем, попал ли пользователь в игровое поле
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    // Если клик вне области игры, сбрасываем координаты
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT: // Обработчик событий окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size(); // Перерисовываем окно при изменении размера
                        break;
                    }
                }
                // Если получен не "ОК", выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        // Возвращаем ответ и координаты клетки
        return { resp, xc, yc };
    }

    // Метод для ожидания взаимодействия пользователя (например, клика по кнопке "повтор")
    Response wait() const
    {
        SDL_Event windowEvent; // Объект для обработки событий SDL
        Response resp = Response::OK; // Переменная для хранения ответа

        while (true) // Бесконечный цикл ожидания событий
        {
            if (SDL_PollEvent(&windowEvent)) // Проверяем, есть ли событие в очереди
            {
                switch (windowEvent.type) // Определяем тип события
                {
                case SDL_QUIT: // Если игрок закрыл окно, завершаем игру
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED: // Обработчик изменения размеров окна
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: { // Обработчик клика мыши
                    int x = windowEvent.motion.x; // Получаем координаты клика
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // Проверяем, нажал ли пользователь кнопку "повтор"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                                        break;
                }
                // Если получен не "ОК", выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        return resp; // Возвращаем ответ
    }

private:
    Board* board; // Указатель на объект игрового поля (используется для вычислений координат)
};
