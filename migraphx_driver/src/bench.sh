#!/bin/bash
MIGX=${MIGX:="/home/mev/source/migraphx_sample/migraphx_driver/build/migx"}
ONNXDIR=${ONNXDIR:="/home/mev/source/migraphx_onnx"}
OUTFILE=outfile

rocminfo=`/opt/rocm/bin/rocminfo | grep gfx | head -1 | awk '{ print $2 }'`
echo "Date",`date '+%F'`
echo "Arch",$rocminfo
echo
echo "Status,Model,Frozen,Batch,Quant,IPS"

while read model quant batch onnx extra
do
    if [ "${quant}" == "fp16" ]; then
	quant_option="--fp16"
    elif [ "${quant}" == "int8" ]; then
	quant_option="--int8"
    fi

    echo "DEBUG " ${MIGX} --onnx ${ONNXDIR}/${onnx} ${quant_option} --perf_report
    ${MIGX} --onnx ${ONNXDIR}/${onnx} ${quant_option} ${extra} --perf_report > ${OUTFILE} 2>&1
    if grep "Rate: " ${OUTFILE} > lastresult; then
	rate=`awk -F'[ /]' '{ print $2 }' lastresult`
	echo "DEBUG Rate = " $rate
	imagepersec=`echo $rate \* $batch | bc`
	echo "DEBUG IPS  = " $imagepersec
	echo PASS,$model,"ONNX",$batch,$quant,$imagepersec 
    else
	imagepersec="0"
	echo FAIL,$model,"ONNX",$batch,$quant,$imagepersec 
    fi
done <<BENCHCONFIG
resnet50         fp32 1 torchvision/resnet50i1.onnx
alexnet          fp32 1 torchvision/alexneti1.onnx
densenet121      fp32 1 torchvision/densenet121i1.onnx
dpn92            fp32 1 cadene/dpn92i1.onnx
fbresnet152      fp32 1 cadene/fbresnet152i1.onnx
resnext101_64x4d fp32 1 cadene/resnext101_64x4di1.onnx
inceptionv3      fp32 1 torchvision/inceptioni1.onnx
vgg16            fp32 1 torchvision/vgg16i1.onnx
wlang/gru        fp32 1 wlang/wlang_gru.onnx --argname=input.1
wlang/lstm       fp32 1 wlang/wlang_lstm.onnx --argname=input.1
bert/bert_mrpc1  fp32 1 bert/bert_mrpc1.onnx --glue=MRPC --gluefile=/home/mev/source/migraphx_sample/migraphx_driver/glue/MRPC.tst
resnet50         fp16 1 torchvision/resnet50i1.onnx
alexnet          fp16 1 torchvision/alexneti1.onnx
densenet121      fp16 1 torchvision/densenet121i1.onnx
dpn92            fp16 1 cadene/dpn92i1.onnx
fbresnet152      fp16 1 cadene/fbresnet152i1.onnx
resnext101_64x4d fp16 1 cadene/resnext101_64x4di1.onnx
inceptionv3      fp16 1 torchvision/inceptioni1.onnx
vgg16            fp16 1 torchvision/vgg16i1.onnx
wlang/gru        fp16 1 wlang/wlang_gru.onnx  --argname=input.1
wlang/lstm       fp16 1 wlang/wlang_lstm.onnx  --argname=input.1
bert/bert_mrpc1  fp16 1 bert/bert_mrpc1.onnx  --glue=MRPC --gluefile=/home/mev/source/migraphx_sample/migraphx_driver/glue/MRPC.tst
resnet50         fp32 64 torchvision/resnet50i1.onnx
alexnet          fp32 64 torchvision/alexneti1.onnx
densenet121      fp32 32 torchvision/densenet121i1.onnx
dpn92            fp32 32 cadene/dpn92i1.onnx
fbresnet152      fp32 32 cadene/fbresnet152i1.onnx
resnext101_64x4d fp32 16 cadene/resnext101_64x4di1.onnx
inceptionv3      fp32 32 torchvision/inceptioni1.onnx
inceptionv4      fp32 16 cadene/inceptionv4i1.onnx
vgg16            fp32 16 torchvision/vgg16i1.onnx
bert/bert_mrpc8  fp32 8 bert/bert_mrpc1.onnx  --glue=MRPC --gluefile=/home/mev/source/migraphx_sample/migraphx_driver/glue/MRPC.tst
resnet50         fp16 64 torchvision/resnet50i1.onnx
alexnet          fp16 64 torchvision/alexneti1.onnx
densenet121      fp16 32 torchvision/densenet121i1.onnx
dpn92            fp16 32 cadene/dpn92i1.onnx
fbresnet152      fp16 32 cadene/fbresnet152i1.onnx
resnext101_64x4d fp16 16 cadene/resnext101_64x4di1.onnx
inceptionv3      fp16 32 torchvision/inceptioni1.onnx
inceptionv4      fp16 16 cadene/inceptionv4i1.onnx
vgg16            fp16 16 torchvision/vgg16i1.onnx
bert/bert_mrpc8  fp16 1 bert/bert_mrpc1.onnx  --glue=MRPC --gluefile=/home/mev/source/migraphx_sample/migraphx_driver/glue/MRPC.tst
BENCHCONFIG
