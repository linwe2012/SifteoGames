import cv2
import os

inputfile = 'You_win.mp4'
outputfolder = './suc'


if  not os.path.exists(outputfolder):
    os.mkdir(outputfolder)

cap = cv2.VideoCapture(inputfile)
i = 0

while cap.isOpened():
    ret, frame = cap.read()
    ret, frame = cap.read()
    ret, frame = cap.read()
    ret, frame = cap.read()

    if ret == True:
        m = frame.shape[0]
        u = frame.shape[1] - m
        u = (int)(u / 2)

        img = frame[0:m, u:u+m]
        img = cv2.resize(img, (128, 128))
        cv2.imwrite(os.path.join(outputfolder, '%02d.png' % i), img)
        i = i + 1
    else:
        break

cap.release()

