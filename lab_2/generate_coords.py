#!/usr/bin/env python3
import random
import sys

def generate_coordinates(num_points, min_val=0, max_val=100):
    """Генерирует случайные координаты для тестирования."""
    coordinates = []
    for _ in range(num_points):
        x = random.uniform(min_val, max_val)
        y = random.uniform(min_val, max_val)
        z = random.uniform(min_val, max_val)
        coordinates.append((x, y, z))
    return coordinates

def write_to_file(coordinates, filename="coordinates_data.c"):
    """Записывает координаты в файл coordinates_data.c."""
    num_points = len(coordinates)
    with open(filename, 'w') as f:
        f.write('#include "coordinates.h"\n\n')
        f.write(f'#define NUM_POINTS {num_points}\n\n')
        f.write('float coordinates[NUM_POINTS][3] = {\n')
        for coord in coordinates:
            f.write("    {{{:.2f}f, {:.2f}f, {:.2f}f}},\n".format(coord[0], coord[1], coord[2]))
        f.write('};\n\n')
        f.write(f'int num_points = {num_points};\n')

def main():
    if len(sys.argv) != 2:
        print("Использование: python generate_coords.py <num_points>")
        sys.exit(1)

    try:
        num_points = int(sys.argv[1])
        if num_points <= 0 or num_points > 1000000:
            raise ValueError("Количество точек должно быть от 1 до 1000000")
    except ValueError as e:
        print(f"Ошибка: {e}")
        sys.exit(1)

    coordinates = generate_coordinates(num_points)
    write_to_file(coordinates)
    print(f"Координаты записаны в coordinates_data.c")

if __name__ == "__main__":
    main()