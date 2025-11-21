if [ $# -lt 3 ]; then
    echo "Использование: $0 <num_books> <num_readers> <num_observers>"
    echo "  num_books: количество книг в библиотеке"
    echo "  num_readers: количество читателей"
    echo "  num_observers: количество наблюдателей"
    exit 1
fi

NUM_BOOKS=$1
NUM_READERS=$2
NUM_OBSERVERS=$3

echo "=========================================="
echo "Запуск библиотечной системы"
echo "=========================================="
echo "Книг: $NUM_BOOKS"
echo "Читателей: $NUM_READERS"
echo "Наблюдателей: $NUM_OBSERVERS"
echo "=========================================="

./cleanup.sh 2>/dev/null

echo "Сборка проекта..."
make clean
make

if [ $? -ne 0 ]; then
    echo "Ошибка сборки!"
    exit 1
fi

echo ""
echo "Запуск библиотекаря..."
./librarian $NUM_BOOKS &
LIBRARIAN_PID=$!
sleep 1

echo "Запуск наблюдателей..."
OBSERVER_PIDS=()
for i in $(seq 1 $NUM_OBSERVERS); do
    xterm -e "./observer $i" &
    OBSERVER_PIDS+=($!)
    sleep 0.3
done

sleep 1

echo "Запуск читателей..."
READER_PIDS=()
for i in $(seq 1 $NUM_READERS); do
    ./reader $i $NUM_BOOKS &
    READER_PIDS+=($!)
    sleep 0.5
done

echo ""
echo "=========================================="
echo "Система запущена!"
echo "Библиотекарь PID: $LIBRARIAN_PID"
echo "Наблюдатели PIDs: ${OBSERVER_PIDS[@]}"
echo "Читатели PIDs: ${READER_PIDS[@]}"
echo ""
echo "Нажмите Enter для остановки..."
echo "=========================================="

read

echo "Остановка процессов..."
kill $LIBRARIAN_PID 2>/dev/null
for pid in "${READER_PIDS[@]}"; do
    kill $pid 2>/dev/null
done
for pid in "${OBSERVER_PIDS[@]}"; do
    kill $pid 2>/dev/null
done

sleep 2

kill -9 $LIBRARIAN_PID 2>/dev/null
for pid in "${READER_PIDS[@]}"; do
    kill -9 $pid 2>/dev/null
done
for pid in "${OBSERVER_PIDS[@]}"; do
    kill -9 $pid 2>/dev/null
done

./cleanup.sh

echo "Система остановлена"

