#
# bench.py - benchmark script for models
#
import argparse
import numpy as np
import cv2 as cv
import time

parser = argparse.ArgumentParser()
parser.add_argument("--framework",default='migraphx',choices=('migraphx','tensorflow'))
parser.add_argument("--save_file",type=str)
parser.add_argument("--image_file",type=str)
parser.add_argument("--resize_val",default=224)
parser.add_argument("--repeat",default=1000)
args=parser.parse_args()

framework=args.framework
save_file=args.save_file
image_file=args.image_file
resize_val=args.resize_val
repeat=args.repeat

def tf_load_graph(save_file):
    # load the graph
    with tf.io.gfile.GFile(save_file,'rb') as f:
        graph_def = tf.compat.v1.GraphDef()
        graph_def.ParseFromString(f.read())
    with tf.Graph().as_default() as graph:
        tf.import_graph_def(graph_def)
    return graph

def load_image(image_file):
    img = cv.imread(image_file)
    img = cv.resize(img,dsize=(resize_val,resize_val))
    if framework == 'migraphx':
        img = img.transpose(2,0,1)
    
    np_img = np.asarray(img)
    np_img_nchw = np.ascontiguousarray(
        np.expand_dims(np_img.astype('float32')/256.0,axis=0))
    return np_img_nchw

if framework == 'tensorflow':
    import tensorflow as tf
    graph = tf_load_graph(save_file)

    # for op in graph.get_operations():
    #    print(op.name)

    # mobilenet for now
    x = graph.get_tensor_by_name('import/input:0')
    y = graph.get_tensor_by_name('import/MobilenetV2/Predictions/Reshape_1:0')

    image = load_image(image_file)
    
    with tf.compat.v1.Session(graph=graph) as sess:
        # dry run just as long as normal
        for i in range(repeat):
            y_out = sess.run(y,feed_dict={x:image})        
        start_time = time.time()
        for i in range(repeat):
            y_out = sess.run(y,feed_dict={x:image})
        finish_time = time.time()
        result = np.array(y_out)
        idx = np.argmax(result[0])
        print('Tensorflow: ')
        print('IDX  = ',idx)
        print('Time = ', '{:8.3f}'.format(finish_time - start_time))
elif framework == 'migraphx':
    import migraphx
    graph = migraphx.parse_tf(save_file)
    graph.compile(migraphx.get_target("gpu"))
    # allocate space with random params
    params = {}
    for key,value in graph.get_parameter_shapes().items():
        params[key] = migraphx.allocate_gpu(value)

    image = load_image(image_file)
    for i in range(repeat):    
        params['input'] = migraphx.to_gpu(migraphx.argument(image))
        result = np.array(migraphx.from_gpu(graph.run(params)),copy=False)
    start_time = time.time()
    for i in range(repeat):    
        params['input'] = migraphx.to_gpu(migraphx.argument(image))
        result = np.array(migraphx.from_gpu(graph.run(params)),copy=False)
    finish_time = time.time()
    idx = np.argmax(result[0])
    print('MIGraphX: ')    
    print('IDX  = ',idx)
    print('Time = ', '{:8.3f}'.format(finish_time - start_time))