//============================================================================
// Name        : Lab2_Task1.cpp
// Author      : Alexander Lemutov
// Version     :
// Copyright   : Free copyright
// Description : Lab2 Task 1
//============================================================================

#include <iostream>
#include <thread>
#include <random>
#include <semaphore>
#include <queue>
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>

const int loading_crane = 5;
const int truck = 15;
const int max_waiting_queue = 5;
const int emergency_queue = 3;

//loading_crane - кол-во кранов, truck - кол-во грузовиков, max_waiting_queue - кол-во грузовиков,
//при котором запускается резервный кран, emergency_queue - кол-во грузовиков,при котором запускается аварийная загрузка

std::counting_semaphore<6> crane(5);
std::mutex queue_mutex;
std::queue<int> waiting_trucks;

std::mutex output_mutex;

std::atomic<bool> emergency_mode = false;
std::atomic<bool> reserve_crane_enabled = false;

void load(int id){
	{
		std::unique_lock lock(queue_mutex);
		waiting_trucks.push(id);
	}

	{
		std::lock_guard<std::mutex> lock(output_mutex);
		std::cout << "Начинается загрузка грузовика " << id << " ." << std::endl;
	}

	if ((reserve_crane_enabled == false) && (waiting_trucks.size() > max_waiting_queue)) {
		reserve_crane_enabled = true;
		crane.release(1);
		std::cout << "Резервный кран запущен. " << std::endl;
	}

	//В строках выше происходит:
	// - добавление грузовика в очередь ожидания, причем добавление происходит только после захвата мьютекса на
	// очередь ожидания;
	// - вывод сообщения о начале загрузке грузовика (после захвата мьютекса на вывод, как в задачах с РК)
	// - запуск резервного крана - сравнение количества грузовиков в очереди со значением, при котором запускается
	// новый кран, для запуска крана освобождается еще один счетный семафор
	// Команды, связанные с семафором, выделяются эклипсом красным, но выполняются, для этого надо прописать использование
	// С++ 20 (по умолчанию используется C++ 17)


	crane.acquire();

	if (waiting_trucks.size() <= emergency_queue){
		emergency_mode = true;
		std::cout << "Запущена аварийная загрузка (ускоренный режим). " << std::endl;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> load_time(1, 3);
		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			waiting_trucks.pop();
		}
		std::this_thread::sleep_for(std::chrono::seconds(load_time(gen)));
		{
			std::lock_guard<std::mutex> lock(output_mutex);
			std::cout << "Грузовик " << id << " загружен. " << std::endl;
		}
	} else {
		emergency_mode = false;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> load_time(3, 6);
		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			waiting_trucks.pop();
		}
		std::this_thread::sleep_for(std::chrono::seconds(load_time(gen)));
		{
			std::lock_guard<std::mutex> lock(output_mutex);
			std::cout << "Грузовик " << id << " загружен. " << std::endl;
		}
	}

	crane.release();

	// в строках выше происходит "загрузка" грузовиков: определяется кол-во грузовиков в очереди, если их не более
	// количества, при котором начинается ускоренная загрузка, то начинается ускоренная загрузка; создается random
	// device, для ускоренного режима временные рамки меньше, чем для обычного. Захватывается мьютекс на очередь
	// ожидания, грузовик удаляется из очереди, процесс останавливается на время загрузки грузовика, затем захватывается
	// мьютекс на вывод и выводится сообщение о загруженном грузовике. Для обычного режима - аналогично, изменяется
	// только время загрузки. После завершения загрузки все семафоры и мьютексы освобождаются.

}

int main() {
	std::vector<std::thread> threads_trucks;
	for (int i = 1; i <= truck; ++i) {
		threads_trucks.emplace_back(load, i);
	}
	for (auto &t: threads_trucks) t.join();
	return 0;
}

//В строках выше создается вектор потоков, симулирующих грузовики, в цикле все грузовики добавляются в вектор потоков
// (а затем - в очередь), затем ожидание завершение выполнения всех потоков.
