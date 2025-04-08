//============================================================================
// Name        : Lab2_Task2.cpp
// Author      : Alexander Lemutov
// Version     :
// Copyright   : Free copyright
// Description : Lab2 Task2
//============================================================================

#include <iostream>
#include <semaphore>
#include <mutex>
#include <chrono>
#include <queue>
#include <atomic>
#include <vector>
#include <thread>
#include <random>

const int threads = 6;
const int quantity_accelerators = 3;

//threads - кол-во потоков, получающих данные, quantity_accelerators - кол-во ускорителей.

std::atomic<bool> accelerator_broken_condition = false;
std::atomic<bool> accelerator_broken_command = false;

//две условные переменные: accelerator_broken_condition - состояние ускорителя,
//accelerator_broken_command - "команда", ломающая ускоритель. Почему их две - будет объяснено позже.

std::mutex output_mutex;

struct Frame {
	int id;
	int priority;
};

//Структура Кадр, включающая номер кадра и его приоритет.

struct ComparePriority {
	bool operator()(const Frame& f1, Frame& f2) {
		return f1.priority > f2.priority;
	}
};

//Структура Сравнения кадров, сравнивает приоритет кадров, необходима, чтобы в очереди с приоритетами
//был приоритет у кадров с меньшим значением priority (1 - более важный, 2 - менее важный)

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> process_time(2, 3);

//Время обработки кадров - случайное, от 2 до 3 секунд.

std::priority_queue<Frame, std::vector<Frame>, ComparePriority> frame_queue;

//Создается очередь с приоритетами: тип хранимых элементов - Frame, тип контейнера - Vector,
//тип сравнения - ComparePriority.

std::counting_semaphore<3> accelerators(3);

//Создается 3 счетных семафора, симулирующих 3 ускорителя. За них будут конкурировать 6 потоков.

void processing_frame(Frame frame){

	if ((accelerator_broken_condition == false) and (accelerator_broken_command == true)) {
		accelerators.acquire();
		accelerator_broken_condition = true;
		std::lock_guard<std::mutex> lock(output_mutex);
		std::cout << "Один ускоритель сломан. " << std::endl;
	}

	//Начало функции обработки кадра (! не всех кадров, а только одного конкретного).
	//Здесь же симулируется режим сломанного ускорителя - если до этого он не был сломан
	//(accelerator_broken_condition == false) и была подана команда на поломку ускорителя
	//(accelerator_broken_command == true), то его состояние меняется (accelerator_broken_condition = true)
	//и захватывается один семафор. Притом команды "release" после этого не подается. Сравнение именно двух
	//переменных нужно, чтоб захватить один семафор только один раз, то есть чтоб каждый раз при обработке
	//нового кадра не "ломался" еще один ускоритель (т.к. ломается только один ускоритель).
	//После поломки состояние ускорителя изменится, команда останется той же, и эти значения уже не будут
	//удовлетворять условию if, то есть поломки еще одного ускорителя не произойдет.

	accelerators.acquire();
	std::this_thread::sleep_for(std::chrono::seconds(process_time(gen)));
	{
		std::lock_guard<std::mutex> lock(output_mutex);
		std::cout << "Кадр " << frame.id << " обработан. Приоритет: " << frame.priority << ". " << std::endl;
	}
	accelerators.release();

	//Здесь захватывается семафор ускорителя (не поломка, а обычная занятость ускорителя для обработки),
	//поток останавливается на время обработки, затем выводится сообщение об обработке кадра (после захвата
	//мьютекса на вывод), после чего освобождается семафор ускорителя (обработка кадра завершена).
}

void processing(){
	while (frame_queue.empty() == false) {
		Frame frame = frame_queue.top();
		frame_queue.pop();
		processing_frame(frame);
	}
}

//функция обработки всего потока кадров: пока очередь кадров не пустая, из очереди достается самый верхний
//элемент (верхним будет элемент с наименьшим значением приоритета (как самый важный)), удаляется из
//очереди и подается на обработку кадра, описанную выше.

void add_frame_to_queue(int id, int priority){
	Frame frame = {id, priority};
	frame_queue.push(frame);
	{
		std::lock_guard<std::mutex> lock(output_mutex);
		std::cout << "Кадр " << frame.id << " с приоритетом " << frame.priority << " добавлен в очередь. " << std::endl;
	}
}

//функция добавления кадра в очередь с приоритетами: формируется кадр с номером и приоритетом, добавляется
//в очередь с приоритетами, сообщение об этом выводится в консоль.

int main() {
	int quantity_frames = 18; //количество кадров - общий поток кадров, который будет получать 6 потоков.

	for (int i = 1; i <= quantity_frames; ++i){
		add_frame_to_queue(i, ((i%2)+1)); //второй аргумент - приоритет: у четный кадров (i%2) == 0, 0 + 1 = 1 - 1й приоритет, у нечетных - 1+1 = 2 - 2й приоритет.
	}

	//В цикле выше все кадры добавляются в очередь с приоритетом

	accelerator_broken_command = true; //команда, отвечающая за сломанный ускоритель.
	//true - ускоритель сломан, false - не сломан.

	std::vector<std::thread> threads_frames; //создается вектор потоков

	{
		std::lock_guard<std::mutex> lock(output_mutex);
		std::cout << "Начинается обработка видеоданных. " << std::endl;
	}

	auto start = std::chrono::high_resolution_clock::now();

	for (int i = 1; i <= threads; ++i){
		threads_frames.emplace_back(processing);
	}
	for (auto &t: threads_frames) t.join();

	//В строках выше засекается время начала обработки, каждому потоку передается функция обработки потока кадров,
	//затем ожидается завершение выполнения всех потоков.
	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "Обработка данных завершена. " << std::endl;
	std::cout << "Время обработки данных: " << std::chrono::duration<double>(end - start).count() << " сек." << std::endl;

	return 0;
}

//По нескольким запускам программы со сломанным ускорителем и с рабочим ускорителем можно отметить,
//что время выполнения заметно отличается. Так, с рабочим ускорителем оно составляет ~16.04 - 16.07 сек,
//а со сломанным - ~21.08 - 24.08 сек.
