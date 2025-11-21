echo "Очистка системных ресурсов..."

for i in {0..99}; do
    sem_unlink="/dev/shm/sem.book_sem_$i"
    if [ -e "$sem_unlink" ]; then
        rm -f "$sem_unlink" 2>/dev/null
    fi
done

rm -f /dev/shm/library_shm* 2>/dev/null

rm -rf /tmp/library_observers 2>/dev/null

echo "Очистка завершена"

