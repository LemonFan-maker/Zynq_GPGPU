import torch
import torch.nn as nn
import torch.nn.functional as F
import torchvision
import torchvision.transforms as transforms
import numpy as np
import os

class PaddedLeNet(nn.Module):
    def __init__(self):
        super(PaddedLeNet, self).__init__()
        # Input: 4x28x28 (1 real + 3 padded zeros)
        self.conv1 = nn.Conv2d(4, 16, kernel_size=5, stride=1, padding=0, bias=True)
        # out: 16x24x24
        self.pool1 = nn.MaxPool2d(kernel_size=2, stride=2)
        # out: 16x12x12
        self.conv2 = nn.Conv2d(16, 16, kernel_size=5, stride=1, padding=0, bias=True)
        # out: 16x8x8
        self.pool2 = nn.MaxPool2d(kernel_size=2, stride=2)
        # out: 16x4x4 = 256
        self.fc1 = nn.Linear(256, 128, bias=True)
        self.fc2 = nn.Linear(128, 16, bias=True)

    def forward(self, x):
        x = F.relu(self.conv1(x))
        x = self.pool1(x)
        x = F.relu(self.conv2(x))
        x = self.pool2(x)
        x = torch.flatten(x, 1)
        x = F.relu(self.fc1(x))
        x = self.fc2(x)
        return x

def pad_mnist(x):
    padded = torch.zeros((4, 28, 28))
    padded[0] = x[0]
    return padded

def train():
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = PaddedLeNet().to(device)
    
    transform = transforms.Compose([
        transforms.ToTensor(),
        transforms.Lambda(pad_mnist)
    ])
    
    train_dataset = torchvision.datasets.MNIST(root='./data', train=True, download=True, transform=transform)
    test_dataset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)
    
    train_loader = torch.utils.data.DataLoader(train_dataset, batch_size=64, shuffle=True)
    test_loader = torch.utils.data.DataLoader(test_dataset, batch_size=1000, shuffle=False)
    
    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    
    print("Training LeNet...")
    for epoch in range(3):
        model.train()
        for batch_idx, (data, targets) in enumerate(train_loader):
            data, targets = data.to(device), targets.to(device)
            optimizer.zero_grad()
            outputs = model(data)
            loss = criterion(outputs, targets)
            loss.backward()
            optimizer.step()
            
            if batch_idx % 200 == 0:
                print(f"Epoch {epoch+1} | Batch {batch_idx}/{len(train_loader)} | Loss: {loss.item():.4f}")
                
        model.eval()
        correct = 0
        total = 0
        with torch.no_grad():
            for data, targets in test_loader:
                data, targets = data.to(device), targets.to(device)
                outputs = model(data)
                _, predicted = torch.max(outputs.data, 1)
                total += targets.size(0)
                correct += (predicted == targets).sum().item()
        print(f"Test Accuracy: {100.0 * correct / total:.2f}%")
    
    print("Saving model weights...")
    os.makedirs('npz', exist_ok=True)
    
    weights = {}
    for name, param in model.named_parameters():
        weights[name] = param.detach().cpu().numpy()
        
    np.savez('npz/lenet_weights.npz', **weights)
    print("Saved to npz/lenet_weights.npz")

if __name__ == '__main__':
    train()
