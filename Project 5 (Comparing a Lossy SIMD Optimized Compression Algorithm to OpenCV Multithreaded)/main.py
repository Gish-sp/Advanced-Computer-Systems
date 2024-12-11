import os
import threading
from PIL import Image
import cupy as cp
import numpy as np
import time
from queue import Queue


def compress_image_single_threaded(input_path, output_path, quality=20):
    """
    Compress an image in a single thread.
    """
    img = Image.open(input_path)
    img.save(output_path, "JPEG", quality=quality)


def compress_image_multithreaded(input_paths, output_dir, num_threads, quality=20):
    """
    Compress images using multithreading with a specified number of threads.
    """
    def worker():
        while True:
            input_path = task_queue.get()
            if input_path is None:
                break
            filename = os.path.basename(input_path)
            output_path = os.path.join(output_dir, filename)
            compress_image_single_threaded(input_path, output_path, quality)
            task_queue.task_done()

    task_queue = Queue()
    threads = []

    # Start threads
    for _ in range(num_threads):
        thread = threading.Thread(target=worker)
        thread.start()
        threads.append(thread)

    # Add tasks to the queue
    for input_path in input_paths:
        task_queue.put(input_path)

    # Wait for all tasks to complete
    task_queue.join()

    # Stop threads
    for _ in range(num_threads):
        task_queue.put(None)
    for thread in threads:
        thread.join()


def compress_image_gpu(input_path, output_path, quality=20):
    """
    Compress an image using GPU acceleration.
    """
    img = Image.open(input_path)
    img_array = np.array(img)
    
    # Convert image array to CuPy array for GPU processing
    gpu_array = cp.array(img_array)
    processed_array = gpu_array * (quality / 100.0)  # Simulated compression on GPU
    processed_array = cp.clip(processed_array, 0, 255).astype(cp.uint8)
    
    # Transfer back to CPU for saving
    result_array = cp.asnumpy(processed_array)
    result_image = Image.fromarray(result_array)
    result_image.save(output_path, "JPEG", quality=quality)


if __name__ == "__main__":
    # Example usage
    input_image = "input.jpg"  # Replace with your image path
    output_dir = "compressed_images"
    os.makedirs(output_dir, exist_ok=True)

    # Single-threaded compression
    start_time = time.time()
    compress_image_single_threaded(input_image, os.path.join(output_dir, "single_threaded.jpg"))
    print(f"Single-threaded compression time: {time.time() - start_time:.2f} seconds")

    # Multithreaded compression
    input_images = [input_image] * 8  # Simulate compressing the same image multiple times
    num_threads = 4  # Specify the number of threads
    start_time = time.time()
    compress_image_multithreaded(input_images, output_dir, num_threads)
    print(f"Multithreaded compression time with {num_threads} threads: {time.time() - start_time:.2f} seconds")

    # GPU-accelerated compression
    start_time = time.time()
    compress_image_gpu(input_image, os.path.join(output_dir, "gpu_compressed.jpg"))
    print(f"GPU-accelerated compression time: {time.time() - start_time:.2f} seconds")
