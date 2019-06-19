/*
 * migx - AMDMIGraphX driver program
 *
 * This driver program provides simple command line options for exercising
 * library calls associated with AMDMIGraphX graph library functions.
 * Included are options for the following stages of processing:
 *
 * 1. Loading saved models
 *
 * 2. Load input data used by the model
 *
 * 3. Quantize the model
 *
 * 4. Compile the program
 *
 * 5. Run program in various configurations
 *
 * More details about each of these options found with usage statement below.
 */
#define QUANTIZATION 1
#include <iostream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <migraphx/onnx.hpp>
#include <migraphx/tf.hpp>
#include <migraphx/gpu/target.hpp>
#include <migraphx/gpu/hip.hpp>
#include <migraphx/cpu/target.hpp>
#include <migraphx/generate.hpp>
#include <migraphx/context.hpp>
#ifdef QUANTIZATION
#include <migraphx/quantization.hpp>
#endif
#include "migx.hpp"
using namespace migraphx;
std::string migx_program; // argv[0] of this process
std::string usage_message =
  migx_program + " <options list>\n" + 
  "where <options list> includes options for\n" +
  "    general use\n" +
  "        --help\n" +
  "        --verbose\n" +
  "        --gpu                run GPU mode (default)\n" +  
  "        --cpu                run CPU mode rather than GPU\n" +
  "        --trace_compile      turn on MIGRAPHX_TRACE_COMPILE\n" +
  "        --trace_eval         turn on MIGRAPHX_TRACE_EVAL\n" +
  "    loading saved models (either --onnx or --tfpb are required)\n" + 
  "        --onnx=<filename>\n" +
  "        --tfpb=<filename>\n" +
  "        --nhwc               set the data layout (--tfpb only)\n" +
  "        --nchw               set the data layout (default)\n" +
  "    quantization\n" +
  "        --fp16               quantize operations to float16\n" +
  "        --int8               quantize operations to int8\n" +
  "    input data\n" +
  "        --imagefile=<filename>\n"
  "        --random_input       create random input for argument\n"
  "        --debugfile=<filename> ASCII file of floating numbers\n" +
  "    running\n" +
  "        --perf_report        run migraphx perf report including times per instruction\n" +
  "        --benchmark          run model repeatedly and time results\n" +
  "        --imageinfo          run model once and report top5 buckets for an image\n" +
  "        --imagenet=<dir>     run model on an imagenet directory\n" +
  "        --mnist=<dir>        run model on mnist directory\n"
  "        --print_model        show MIGraphX instructions for model\n" +
  "        --eval               run model and dump output to stdout\n" +
  "        --trim=<n>           trim program to remove last <n> instructions\n" +
  "        --iterations=<n>     set iterations for perf_report and benchmark (default 1000)\n" +
  "        --copyarg            copy arguments in and results back (--benchmark only)\n" +
  "        --argname=<name>     set name of model input argument (default 0)\n";

bool is_verbose = false;
bool is_gpu = true;
enum model_type { model_unknown, model_onnx, model_tfpb } model_type = model_unknown;
std::string model_filename;
bool is_nhwc = true;
bool set_nhwc = false;
enum quantize_type { quantize_none, quantize_fp16, quantize_int8 } quantize_type = quantize_none;
enum run_type { run_none, run_benchmark, run_perfreport, run_imageinfo, run_imagenet, run_mnist, run_printmodel, run_eval, run_eval_print } run_type = run_none;
int iterations = 1000;
bool copyarg = false;
std::string argname = "0";
enum fileinput { fileinput_none, fileinput_image, fileinput_debug } fileinput_type = fileinput_none;
std::string image_filename;
std::string debug_filename;
std::string imagenet_dir;
std::string mnist_dir;
int mnist_images;
int trim_instructions = 0;
bool trace_eval = false;
std::string trace_eval_var = "MIGRAPHX_TRACE_EVAL=1";
bool trace_compile = false;
std::string trace_compile_var = "MIGRAPHX_TRACE_COMPILE=1";
bool has_random_input = false;

/* parse_options
 *
 * Parse user options, returning 0 on success
 */
int parse_options(int argc,char *const argv[]){
  int opt;
  static struct option long_options[] = {
    { "help",    no_argument,       0, 1 },
    { "verbose", no_argument,       0, 2 },
    { "gpu",     no_argument,       0, 3 },
    { "cpu",     no_argument,       0, 4 },
    { "trace_eval", no_argument,    0, 5 },
    { "trace_compile", no_argument, 0, 6 },
    { "onnx",    required_argument, 0, 8 },
    { "tfpb",    required_argument, 0, 9 },
    { "nhwc",    no_argument,       0, 10 },
    { "nchw",    no_argument,       0, 11 },
    { "fp16",    no_argument,       0, 12 },
    { "int8",    no_argument,       0, 13 },
    { "imagefile", required_argument, 0, 14 },
    { "random_input", no_argument,  0, 15 },
    { "debugfile", required_argument, 0, 16 },
    { "benchmark", no_argument,     0, 17 },
    { "perf_report", no_argument,   0, 18 },
    { "imageinfo", no_argument,     0, 19 },
    { "imagenet", required_argument, 0, 20 },
    { "mnist", required_argument, 0, 21 },
    { "print_model", no_argument,   0, 22 },
    { "eval", no_argument, 0, 23 },
    { "trim", required_argument, 0, 24 },
    { "iterations", required_argument, 0, 25 },
    { "copyarg", no_argument,       0, 26 },
    { "argname", required_argument, 0, 27 },
  };
  while ((opt = getopt_long(argc,argv,"",long_options,NULL)) != -1){
    switch (opt){
    case 1:
      return 1;
    case 2:
      is_verbose = true;
      break;
    case 3:
      is_gpu = true;
      break;
    case 4:
      is_gpu = false;
      break;
    case 5:
      trace_eval = true;
      break;
    case 6:
      trace_compile = true;
      break;
    case 8:
      model_type = model_onnx;
      model_filename = optarg;
      break;
    case 9:
      model_type = model_tfpb;
      model_filename = optarg;
      break;
    case 10:
      is_nhwc = true;
      set_nhwc = true;
      break;
    case 11:
      is_nhwc = false;
      break;
    case 12:
      quantize_type = quantize_fp16;
      break;
    case 13:
      quantize_type = quantize_int8;
      break;
    case 14:
      fileinput_type = fileinput_image;
      image_filename = optarg;
      break;
    case 15:
      has_random_input = true;
      break;
    case 16:
      fileinput_type = fileinput_debug;      
      debug_filename = optarg;
      break;
    case 17:
      run_type = run_benchmark;
      break;
    case 18:
      run_type = run_perfreport;
      break;
    case 19:
      run_type = run_imageinfo;
      break;
    case 20:
      imagenet_dir = optarg;
      run_type = run_imagenet;
      break;
    case 21:
      mnist_dir = optarg;
      run_type = run_mnist;
      break;
    case 22:
      if (run_type == run_eval)
	run_type = run_eval_print;
      else
	run_type = run_printmodel;
      break;
    case 23:
      if (run_type == run_printmodel)
	run_type = run_eval_print;
      else
	run_type = run_eval;
      break;
    case 24:
      if (std::stoi(optarg) < 0){
	std::cerr << migx_program << ": trim < 0, ignored" << std::endl;	
      } else {
	trim_instructions = std::stoi(optarg);
      }
      break;
    case 25:
      if (std::stoi(optarg) < 0){
	std::cerr << migx_program << ": iterations < 0, ignored" << std::endl;
      } else {
	iterations = std::stoi(optarg);
      }
      break;
    case 26:
      copyarg = true;
      break;
    case 27:
      argname = optarg;
      break;
    default:
      return 1;
    }
  }
  if (model_type == model_unknown){
    std::cerr << migx_program << ": either --onnx or --tfpb must be given" << std::endl;
    return 1;
  }
  if (model_type == model_onnx && set_nhwc && is_nhwc){
    std::cerr << migx_program << ": --onnx is not compatible with --nhwc" << std::endl;
    return 1;
  }  
  if ((run_type == run_imageinfo) && image_filename.empty()){
    std::cerr << migx_program << ": --imageinfo requires --imagefile option" << std::endl;
    return 1;
  }
  return 0;
}

/* get_time
 *
 * return current time in milliseconds
 */
double get_time(){
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return static_cast<double>(tv.tv_usec / 1000) + tv.tv_sec * 1000;
}

template <class T>
auto get_hash(const T& x){
  return std::hash<T>{}(x);
}

int main(int argc,char *const argv[],char *const envp[]){
  migx_program = argv[0];
  if (parse_options(argc,argv)){
    std::cerr << migx_program << ": usage: " << usage_message;
    return 1;
  }

  // load the model file
  if (is_verbose)
    std::cout << "loading model file" << std::endl;
  
  program prog;
  if (model_type == model_onnx){
    try {
      prog = parse_onnx(model_filename);
    } catch(...){
      std::cerr << migx_program << ": unable to load ONNX file " << model_filename << std::endl;
      return 1;
    }
  } else if (model_type == model_tfpb){
    try {
      prog = parse_tf(model_filename,is_nhwc);
    } catch( std::exception &exc){
      std::cerr << exc.what();
    } catch(...){
      std::cerr << migx_program << ": unable to load TF protobuf file " << model_filename << std::endl;
      return 1;
    }
  }

#ifdef QUANTIZATION
  // quantize the program
  if (quantize_type == quantize_fp16){
    quantize(prog);
  } else
#endif
    if (quantize_type != quantize_none){
    std::cerr << "quantization not yet implemented" << std::endl;
  }

  // compile the program
  if (trace_compile){
    putenv((char *) trace_compile_var.c_str());
  }
  if (is_verbose)
    std::cout << "compiling model" << std::endl;
  if (is_gpu)
    prog.compile(migraphx::gpu::target{});
  else
    prog.compile(migraphx::cpu::target{});

  // remove the last "trim=n" instructions, debugging tool with --eval to print out intermediate results
  if (trim_instructions > 0 && trim_instructions < prog.size()){
    auto prog2 = prog;
    // create shorter program removing "trim" instructions in size
    auto last = std::prev(prog2.end(),trim_instructions);
    prog2.remove_instructions(last,prog2.end());
    prog = prog2;
  }

  // set up the parameter map
  program::parameter_map pmap;  
  bool argname_found = false;
  shape argshape;
  int batch_size=1, channels=1, height=1, width=1;  
  for (auto&& x: prog.get_parameter_shapes()){
    if (is_verbose)
      std::cout << "parameter: " << x.first << std::endl;      
    if (x.first == argname){
      argshape = x.second;
      argname_found = true;
    }
    if (is_gpu && has_random_input)
      pmap[x.first] = migraphx::gpu::to_gpu(migraphx::generate_argument(x.second));
    else if (is_gpu)
      pmap[x.first] = migraphx::gpu::allocate_gpu(x.second);
    else
      pmap[x.first] = migraphx::generate_argument(x.second,get_hash(x.first));
  }

  // pattern match argument names and types
  enum image_type img_type;
  if (argname_found == false){
    std::cerr << "input argument: " << argname << " not found, use --argname to pick from following candidates" << std::endl;
    for (auto&& x: prog.get_parameter_shapes()){
      std::cout << "\t" << x.first << std::endl;
    }
    if (run_type != run_printmodel) return 1;
  } else {
    if (is_verbose){
      std::cout << "model input [";
      for (int i=0;i<argshape.lens().size();i++){
	if (i!=0) std::cout << ",";
	std::cout << argshape.lens()[i];
      }
      std::cout << "] " << argshape.elements() << " elements" << std::endl;
    }
    if (argshape.lens().size() == 4){
      batch_size = argshape.lens()[0];
      channels = argshape.lens()[1];
      height = argshape.lens()[2];
      width = argshape.lens()[3];
    } else if (argshape.lens().size() == 3){
      channels = argshape.lens()[0];
      height = argshape.lens()[1];
      width = argshape.lens()[2];
    }
    if (channels == 3 && height == 224 && width == 224) img_type = image_imagenet224;
    else if (channels == 3 && height == 299 && width == 299) img_type = image_imagenet299;
    else if (channels == 1 && height == 28 && width == 28) img_type = image_mnist;
    else img_type = image_unknown;
  }

  // read image data if passed
  //  std::vector<float> image_data(3*height*width);
  std::vector<float> image_data;
  std::vector<float> image_alloc(3*height*width);      
  if (fileinput_type == fileinput_image){
    if (!image_filename.empty()){
      if (is_verbose)
	std::cout << "reading image: " << image_filename << " " << std::endl;
      read_image(image_filename,img_type,image_alloc,false,model_type==model_onnx);
      image_data = image_alloc;
    }
  } else if (fileinput_type == fileinput_debug){
    if (!debug_filename.empty()){
      if (is_verbose)
	std::cout << "reading debug: " << image_filename << " " << std::endl;
      read_float_file(debug_filename,image_data);
    }
    if (image_data.size() < argshape.elements()){
      std::cerr << migx_program << ": model requires " << argshape.elements() << " inputs, only " << image_data.size() << " provided" << std::endl;
      return 1;
    }
  }

  migraphx::argument result;
  migraphx::argument resarg;
  double start_time,finish_time,elapsed_time;
  int top5[5];
  // alternatives for running the program
  if (trace_eval){
    putenv((char *) trace_eval_var.c_str());    
  }
  auto ctx = prog.get_context();
  switch(run_type){
  case run_none:
    // do nothing
    break;
  case run_benchmark:
    int i;
    if (is_verbose && iterations > 1){
      std::cout << "running           " << iterations << " iterations" << std::endl;
    }
    start_time = get_time();
    for (i = 0;i < iterations;i++){
      if (is_gpu){
	if (copyarg)
	  pmap[argname] = migraphx::gpu::to_gpu(generate_argument(prog.get_parameter_shape(argname)));
	resarg = prog.eval(pmap);
	ctx.finish();
	if (copyarg)
	  result = migraphx::gpu::from_gpu(resarg);
      } else {
	resarg = prog.eval(pmap);
	ctx.finish();
      }
    }
    finish_time = get_time();
    elapsed_time = (finish_time - start_time)/1000.0;

    std::cout << "batch size        " << batch_size << std::endl;        
    std::cout << std::setprecision(6) << "Elapsed time(ms): " << elapsed_time << std::endl;
    std::cout << "Images/sec:       " << (iterations*batch_size)/elapsed_time << std::endl;
    break;
  case run_perfreport:
    if (is_verbose && iterations > 1){
      std::cout << "running           " << iterations << " iterations" << std::endl;
    }
    prog.perf_report(std::cout,iterations,pmap);
    break;
  case run_imageinfo:
    if (is_gpu) {
      pmap[argname] = migraphx::gpu::to_gpu(migraphx::argument{
	  pmap[argname].get_shape(),image_data.data()});
    } else {
      pmap[argname] = migraphx::argument{
	pmap[argname].get_shape(),image_data.data()};
    }
    if (is_gpu){
      resarg = prog.eval(pmap);
      result = migraphx::gpu::from_gpu(resarg);
    } else {
      result = prog.eval(pmap);
    }
    if (result.get_shape().elements() == 1001){
      // skip 1st label
      image_top5(((float *) result.data())+1, top5);      
    } else {
      image_top5((float *) result.data(), top5);
    }
    std::cout << "top1 = " << top5[0] << " " << imagenet_labels[top5[0]] << std::endl;
    std::cout << "top2 = " << top5[1] << " " << imagenet_labels[top5[1]] << std::endl;
    std::cout << "top3 = " << top5[2] << " " << imagenet_labels[top5[2]] << std::endl;
    std::cout << "top4 = " << top5[3] << " " << imagenet_labels[top5[3]] << std::endl;
    std::cout << "top5 = " << top5[4] << " " << imagenet_labels[top5[4]] << std::endl;
    break;
  case run_imagenet:
    {
      int count = 0;
      int ntop1 = 0;
      int ntop5 = 0;
      std::string imagefile;
      int expected_result;
      if (chdir(imagenet_dir.c_str()) == -1){
	std::cerr << migx_program << ": can not change to imagenet dir: " << imagenet_dir << std::endl;
	return 1;
      }
      std::fstream index("val.txt");
      if (!index || (index.peek() == EOF)){
	std::cerr << migx_program << ": can not open val.txt: " << imagenet_dir << std::endl;
	return 1;
      }
      while (1){
	index >> imagefile >> expected_result;
	if (index.eof()) break;
	read_image(imagefile,img_type,image_alloc,false,model_type==model_onnx);
	count++;
	if (is_gpu){
	  pmap[argname] = migraphx::gpu::to_gpu(migraphx::argument{
	      pmap[argname].get_shape(),image_alloc.data()});
	  resarg = prog.eval(pmap);
	  result = migraphx::gpu::from_gpu(resarg);
	} else {
	  pmap[argname] = migraphx::argument{
	    pmap[argname].get_shape(),image_alloc.data()};
	  result = prog.eval(pmap);
	}
	if (result.get_shape().elements() == 1001){
	  image_top5(((float *) result.data())+1, top5);
	} else {
	  image_top5((float *) result.data(), top5);
	}
	if (top5[0] == expected_result) ntop1++;
	if (top5[0] == expected_result ||
	    top5[1] == expected_result ||
	    top5[2] == expected_result || 
	    top5[3] == expected_result || 
	    top5[4] == expected_result) ntop5++;
	if (count % 1000 == 0)
	  std::cout << count << " top1: " << ntop1 << " top5: " << ntop5 << std::endl;
      }
      std::cout << "Overall - top1: " << (double) ntop1/count << " top5: " << (double) ntop5/count << std::endl;
    }
    break;
  case run_mnist:
    {
      int i,j;
      
      std::vector<float> image_data(28*28);
      int label;
      float *label_result;
      if (img_type != image_mnist){
	std::cerr << migx_program << ": --mnist requires input size [1,28,28]" << std::endl;
	return 1;
      }
      if (initialize_mnist_streams(mnist_dir,mnist_images)){
	std::cerr << migx_program << ": can not read mnist files in dir: " << mnist_dir << std::endl;
	return 1;
      }
      if (is_verbose)
	std::cout << "mnist images = " << mnist_images << std::endl;
      int total_pass = 0;
      for (i=0;i<mnist_images;i++){
	read_mnist(image_data,label);
	if (is_verbose)
	  ascii_mnist(image_data,label);
	if (is_gpu){
	  pmap[argname] = migraphx::gpu::to_gpu(migraphx::argument{
	      pmap[argname].get_shape(),image_data.data()});
	  resarg = prog.eval(pmap);
	  result = migraphx::gpu::from_gpu(resarg);
	} else {
	  pmap[argname] = migraphx::argument{
	    pmap[argname].get_shape(),image_data.data()};
	  result = prog.eval(pmap);	  
	}
	label_result = (float *) result.data();
	int maxidx=0;
	for (j=0;j<10;j++){
	  if (label_result[j] > label_result[maxidx])
	    maxidx = j;
	}
	if (maxidx == label) total_pass++;
	    
	if (is_verbose){
	  for (j=0;j<10;j++){
	    std::cout << ((j==maxidx)?"*":"") << label_result[j] << " ";
	  }
	  std::cout << std::endl;
	}
      }
      finish_mnist_streams();
      std::cout << "MNIST results: " << total_pass << " / " << mnist_images << " = " << (double) total_pass / mnist_images << std::endl;
    }
    break;
  case run_printmodel:
    std::cout << "Program with " << prog.size() << " instructions" << std::endl;
    std::cout << prog;
    break;
  case run_eval_print:
    std::cout << "Program with " << prog.size() << " instructions" << std::endl;
    std::cout << prog;
    // fallthru
  case run_eval:
    // load argument
    if (is_verbose){
      std::cout << "Inputs: " << std::endl;
      for (int i=0;i < image_data.size();i++)
	std::cout << "\t" << image_data[i] << std::endl;
    }
    if (is_gpu){
      pmap[argname] = migraphx::gpu::to_gpu(migraphx::argument{
	  pmap[argname].get_shape(),image_data.data()});
    } else {
      pmap[argname] = migraphx::argument{
	pmap[argname].get_shape(),image_data.data()};
    }
    // evaluate
    if (is_gpu){
      resarg = prog.eval(pmap);
      result = migraphx::gpu::from_gpu(resarg);
    } else {
      result = prog.eval(pmap);
    }
    std::cout << result << std::endl;
    
  }

  return 0;
}

