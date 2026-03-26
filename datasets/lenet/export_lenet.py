import torch
import numpy as np
import os
import argparse
from train_lenet import PaddedLeNet, pad_mnist
import torchvision
import torchvision.transforms as transforms

def symmetric_int8_quant(x: np.ndarray):
    max_abs = float(np.max(np.abs(x)))
    scale = max(max_abs / 127.0, 1e-12)
    q = np.clip(np.round(x / scale), -127, 127).astype(np.int8)
    return q, np.float32(scale)

def export_c_header(quant_data, filepath):
    with open(filepath, 'w') as f:
        f.write("#ifndef LENET_DATA_H\n")
        f.write("#define LENET_DATA_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        f.write(f"#define LENET_X_SCALE {quant_data['x_scale']:.8f}f\n")
        for layer in ['conv1', 'conv2', 'fc1', 'fc2']:
            f.write(f"#define LENET_{layer.upper()}_W_SCALE {quant_data[layer+'_w_scale']:.8f}f\n")
            f.write(f"#define LENET_{layer.upper()}_OUT_SCALE {quant_data[layer+'_out_scale']:.8f}f\n")
            f.write(f"#define LENET_{layer.upper()}_B_SCALE {quant_data[layer+'_b_scale']:.8f}f\n")
            
        f.write("\n")
        
        def write_array(name, arr, c_type):
            flat = arr.flatten()
            f.write(f"static const {c_type} lenet_{name}[{len(flat)}] = {{\n    ")
            for i, val in enumerate(flat):
                f.write(f"{val},")
                if (i + 1) % 16 == 0:
                    f.write("\n    ")
                else:
                    f.write(" ")
            f.write("\n};\n\n")
            
        write_array("x_q", quant_data['x_q'], "uint8_t")
        write_array("y", quant_data['y'], "uint8_t")
        
        for layer in ['conv1', 'conv2', 'fc1', 'fc2']:
            write_array(f"{layer}_w_q", quant_data[f'{layer}_w_q'], "int8_t")
            write_array(f"{layer}_b_q", quant_data[f'{layer}_b_q'], "int32_t")
            
        f.write("#endif // LENET_DATA_H\n")

def main():
    model = PaddedLeNet()
    model.eval()
    
    npz = np.load('npz/lenet_weights.npz')
    state_dict = {}
    for k, v in npz.items():
        state_dict[k] = torch.from_numpy(v)
    model.load_state_dict(state_dict)
    
    transform = transforms.Compose([
        transforms.ToTensor(),
        transforms.Lambda(pad_mnist)
    ])
    
    test_dataset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)
    test_loader = torch.utils.data.DataLoader(test_dataset, batch_size=32, shuffle=False)
    
    x_batch, y_batch = next(iter(test_loader))
    
    activation_maxes = {}
    
    def get_hook(name):
        def hook(model, input, output):
            relu_out = torch.nn.functional.relu(output)
            max_val = float(torch.max(torch.abs(relu_out)))
            if name not in activation_maxes:
                activation_maxes[name] = max_val
            else:
                activation_maxes[name] = max(activation_maxes[name], max_val)
        return hook

    model.conv1.register_forward_hook(get_hook('conv1'))
    model.conv2.register_forward_hook(get_hook('conv2'))
    model.fc1.register_forward_hook(get_hook('fc1'))
    model.fc2.register_forward_hook(get_hook('fc2'))
    
    with torch.no_grad():
        out = model(x_batch)
    
    quant_data = {}
    
    # Quantize input [0, 1] to [0, 127]
    x_scale = 1.0 / 127.0
    x_q = np.clip(np.round(x_batch.numpy() / x_scale), 0, 127).astype(np.uint8)
    x_q_hwc = np.transpose(x_q, (0, 2, 3, 1))
    
    quant_data['x_q'] = x_q_hwc
    quant_data['y'] = y_batch.numpy().astype(np.uint8)
    quant_data['x_scale'] = np.float32(x_scale)
    
    in_scale = x_scale
    for layer in ['conv1', 'conv2', 'fc1', 'fc2']:
        weight = state_dict[f'{layer}.weight'].numpy()
        bias = state_dict[f'{layer}.bias'].numpy()
        
        if 'conv' in layer:
            weight = np.transpose(weight, (2, 3, 1, 0))
            
        if 'fc' in layer:
            weight = np.transpose(weight, (1, 0))
            
        w_q, w_scale = symmetric_int8_quant(weight)
        
        b_scale = in_scale * w_scale
        b_q = np.round(bias / b_scale).astype(np.int32)
        
        if layer == 'fc2':
            out_scale = 1.0
            for i in range(len(y_batch)):
                pass
        else:
            out_max = activation_maxes[layer]
            out_scale = np.float32(max(out_max / 127.0, 1e-12))
        
        quant_data[f'{layer}_w_q'] = w_q
        quant_data[f'{layer}_b_q'] = b_q
        quant_data[f'{layer}_w_scale'] = np.float32(w_scale)
        quant_data[f'{layer}_b_scale'] = np.float32(b_scale)
        quant_data[f'{layer}_out_scale'] = np.float32(out_scale)
        
        in_scale = out_scale

    np.savez('npz/lenet_gpu_bundle.npz', **quant_data)
    print("Saved NPZ bundle to npz/lenet_gpu_bundle.npz")
    
    export_c_header(quant_data, 'lenet_data.h')
    print("Exported lenet_data.h")
    
if __name__ == '__main__':
    main()
