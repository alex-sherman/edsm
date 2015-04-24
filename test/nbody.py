#!/usr/bin/python

import jrpc
import pygame

server = None
server = jrpc.service.SocketProxy(8764, host = "vector57.net") #The server's listening port

pygame.init()
screen_size = 400
screen = pygame.display.set_mode((screen_size, screen_size))
#points = [[0,0,1], [0,1,1], [1,0,1], [1,1,1]]
points = []
point_count = 2
for x in range(point_count):
    for y in range(point_count):
        points.append([1.0 * x / point_count, 1.0 * y / point_count,1])


b1, b2 = server.init_simulation(points)

print b1, b2

while True:
    screen.fill((0,0,0))
    for point in points:
        pygame.draw.circle(screen, (128,128,128), (int(screen_size * point[0]), int(screen_size *point[1])), int(point[2] * 3), 0)

    pygame.display.update()
    points = server.run_simulation(len(points), b1, b2, 0.01, 1, 1)
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            exit()
