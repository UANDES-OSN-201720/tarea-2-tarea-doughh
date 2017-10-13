import sys
import os


def main():

	if len(sys.argv) != 3:
		print('Missing args <lru|fifo|custom> <scan|focus|sort>')
		return

	algorythm = sys.argv[1]
	program = sys.argv[2]

	print('Frames,Faults,Read,Write')

	for frame in range(2, 101):
		os.system('./virtmem 100 {0} {1} {2}'.format(frame, algorythm, program))

if __name__ == '__main__':
	main()
