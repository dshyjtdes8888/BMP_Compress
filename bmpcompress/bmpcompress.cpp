#include "bmpcompress.h"
#include "ui_bmpcompress.h"
#include<QFileDialog>
#include<iostream>
#include<vector>
#include<string>
#include<fstream>
#include<windows.h>
#include<QMessageBox>
#include<stack>
using namespace std;

BmpCompress::BmpCompress(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BmpCompress)
{
    ui->setupUi(this);
    this->setWindowTitle("灰度图像压缩与解压");
    ui->progressBar->setRange(0,10000);
    ui->progressBar->setValue(0);
}

// 计算像素点p需要的存储位数的函数
int length(int i)
{
    int k = 1;
    i = i / 2;
    while (i>0)
    {
        k++;
        i = i / 2;
    }
        return k;
    }

// 动态规划算法，压缩位图数据
void dp(int n, unsigned int p[], unsigned int s[], unsigned int l[], unsigned int b[])
{
    int Lmax = 256, header = 11;
    s[0] = 0;
    for (int i = 1; i <= n; i++)
    {
        b[i] = length(p[i]);//计算像素点p需要的存储位数
        unsigned int bmax = b[i];
        s[i] = s[i - 1] + bmax;
        l[i] = 1;
        for (int j = 2; j <= i && j <= Lmax; j++)
        {
            if (bmax<b[i - j + 1])
                bmax = b[i - j + 1];
            if (s[i]>s[i - j] + j * bmax)
            {
                s[i] = s[i - j] + j * bmax;
                l[i] = j;
            }
        }
        s[i] += header;
    }
}

// 回溯确定数据分段位置
void Traceback(int n, int& i, unsigned int s[], unsigned int l[])
{
    if (n == 0)
        return;
    Traceback(n - l[n], i, s, l);
    s[i++] = n - l[n];//重新为s[]数组赋值，用来存储分段位置
}

// 输出函数，用于将数据分段后输出
int Output(unsigned int s[], unsigned int l[], unsigned int b[], int n)
{
    int m = 0;
    Traceback(n, m, s, l);
    s[m] = n;
    for (int j = 1; j <= m; j++)
    {
        l[j] = l[s[j]];
        b[j] = b[s[j]];
    }
    return m;
}


bool Compress(string name,Ui::BmpCompress *ui)
{
    ifstream dmp_file(&name[0], ios::binary);
    if (dmp_file)
    {
        unsigned char **data; // 存储像素信息的二维数组
        BITMAPFILEHEADER FileHead; // bmp文件头
        BITMAPINFOHEADER InfoHead; // bmp信息头
        RGBQUAD Color[256]; // bmp颜色表
        dmp_file.read((char*)&FileHead, sizeof(FileHead));
        ui->progressBar->setValue(200);//设置进度条
        if (FileHead.bfType != 19778) // 判断是否为bmp文件
            return 0;
        // 读取信息头和颜色表
        dmp_file.read((char*)&InfoHead, sizeof(InfoHead));
        dmp_file.read((char*)&Color, FileHead.bfOffBits - sizeof(InfoHead) - sizeof(FileHead));
         // 计算像素总数和每行像素字节数
        int count = InfoHead.biWidth * InfoHead.biHeight;//像素总数
        int lineByte = (InfoHead.biWidth * InfoHead.biBitCount / 8 + 3) / 4 * 4;//一行读多少个
        dmp_file.seekg(FileHead.bfOffBits, ios_base::beg);
        ui->progressBar->setValue(500);//设置进度条
        data = new unsigned char*[InfoHead.biHeight];
        for (int i = 0; i < InfoHead.biHeight; i++)//像素信息存到二维数组里
        {
            data[i] = new unsigned char[lineByte];
            dmp_file.read((char*)data[i], lineByte);
        }
        dmp_file.close();
        //数据读完
        ui->progressBar->setValue(1000);//设置进度条
        name += ".dp";
        ofstream dp_file(&name[0], ios::binary);
        if (!dp_file)return 0;
         // 将文件头、信息头和颜色表写入压缩文件中
        dp_file.write((char*)&FileHead, sizeof(FileHead));
        dp_file.write((char*)&InfoHead, sizeof(InfoHead));
        dp_file.write((char*)&Color, FileHead.bfOffBits - sizeof(InfoHead) - sizeof(FileHead));
        ui->progressBar->setValue(1500);//设置进度条
        unsigned int *dataline = new unsigned int[count + 1];
        dataline[0] = 0;
         // 蛇形扫描像素信息，并存储到一维数组中
        unsigned int temp = 1;//这个时候这个变量是下标索引
        for (int i = 0; i < InfoHead.biHeight; i++)
        {
            if (i % 2 == 0)//奇数行
            {
                for (int j = 0; j < lineByte; j++)
                {
                    dataline[temp] = data[i][j];
                    temp++;
                }
            }
            else if (i % 2 != 0)//偶数行
            {
                for (int j = lineByte - 1; j >= 0; j--)
                {
                    dataline[temp] = data[i][j];
                    temp++;
                }
            }
        }

        ui->progressBar->setValue(2000);//设置进度条
        unsigned int *s = new unsigned int[count + 1]; s[0] = 0;
        unsigned int *b = new unsigned int[count + 1]; b[0] = 0;//多少位
        unsigned int *l = new unsigned int[count + 1]; l[0] = 0;//多少个
        dp(count, dataline, s, l, b);
        int PartCount;
        int Begin;
        PartCount= Output(s, l, b, count);
        Begin=1;
        for (int i = 1; i <= PartCount; i++)
        {//对b数组数据重新分析
            int bmax = 0;
            for (unsigned int j = Begin; j < Begin + l[i]; j++)
            {
                bmax = max(bmax, length(dataline[j]));
            }
            Begin += l[i];
            b[i] = bmax;
        }
        ui->progressBar->setValue(2500);//设置进度条
        //写操作
        temp = 0;//这个时候变成了位运算参量
        Begin = 1;//用做访问数组下标
        int track = 0;//用于跟踪
        unsigned CpsDataCount = 0;
        for (int i = 1; i <= PartCount; i++)
        {
            if (track + 8 < 32)
            {
                temp <<= 8;
                temp = temp | (l[i] - 1);
                track += 8;
            }
            else if (track + 8 == 32)
            {
                temp <<= 8;
                temp = temp | (l[i] - 1);
                dp_file.write((char*)&temp, sizeof(temp));
                CpsDataCount++;
                temp = 0; track = 0;
            }
            else// track + 8 > 32
            {
                int t = 32 - track;//8位先存t位
                temp <<= t;
                temp = temp | ((l[i] - 1) >> (8 - t));//存前t位
                dp_file.write((char*)&temp, sizeof(temp));
                CpsDataCount++;
                temp = 0;
                temp = temp | (((l[i] - 1) << (32 - 8 + t)) >> (32 - 8 + t));
                track = 8 - t;
            }//8位已存
            if (track + 3 < 32)
            {
                temp <<= 3;
                temp = temp | (b[i] - 1);
                track += 3;
            }
            else if (track + 3 == 32)
            {
                temp <<= 3;
                temp = temp | (b[i] - 1);
                dp_file.write((char*)&temp, sizeof(temp));
                CpsDataCount++;
                temp = 0; track = 0;
            }
            else// track + 3 > 32
            {
                int t = 32 - track;
                temp <<= t;
                temp = temp | ((b[i] - 1) >> (3 - t));//存前t位
                dp_file.write((char*)&temp, sizeof(temp));
                CpsDataCount++;
                temp = 0;
                temp = temp | (((b[i] - 1) << (32 - 3 + t)) >> (32 - 3 + t));
                track = 3 - t;
            }//3位已存
            for (unsigned int j = 0; j < l[i]; j++)
            {
                if (track + b[i] < 32)
                {
                    temp <<= b[i];
                    temp = temp | dataline[Begin];
                    Begin++;
                    track += b[i];
                }
                else if (track + b[i] == 32)
                {
                    temp <<= b[i];
                    temp = temp | dataline[Begin];
                    Begin++;
                    dp_file.write((char*)&temp, sizeof(temp));
                    CpsDataCount++;
                    temp = 0; track = 0;
                }
                else// track + b[i] > 32
                {
                    int t = 32 - track;//b[i]位先存t位
                    temp <<= t;
                    temp = temp | (dataline[Begin] >> (b[i] - t));//存前t位
                    dp_file.write((char*)&temp, sizeof(temp));
                    CpsDataCount++;
                    temp = 0;
                    temp = temp | ((dataline[Begin] << (32 - b[i] + t)) >> (32 - b[i] + t));
                    Begin++;
                    track = b[i] - t;
                    if (Begin == count + 1)//最后一个数据
                    {
                        temp <<= (32 - track);
                        dp_file.write((char*)&temp, sizeof(temp));
                        CpsDataCount++;
                        temp = 0;
                    }
                }//b[i]位已存
            }
            ui->progressBar->setValue(2500+7500/PartCount*i);//设置进度条
        }
        dp_file.close();
        delete[]data;
        delete[]dataline;
        delete[]s; delete[]b; delete[]l;
        return 1;
    }
    else return 0;
}

//解压缩
bool UnCompress(string name, Ui::BmpCompress* ui)
{
    // 判断文件是否为.dp格式，如果不是则直接返回0表示解压缩失败
    if (name.substr(name.length() - 3) != ".dp")
    {
        return 0;
    }
    // 以二进制方式打开待解压缩文件
    ifstream fin(&name[0], ios::binary);
    // 如果文件存在
    if (fin)
    {
        BITMAPFILEHEADER FileHead; // 位图文件头结构体
        BITMAPINFOHEADER InfoHead; // 位图信息头结构体
        RGBQUAD Color[256]; // 调色板颜色数组
        fin.read((char*)&FileHead, sizeof(FileHead)); // 读取文件头
        ui->progressBar->setValue(200); // 设置进度条
        // 如果不是BMP文件则直接返回0表示解压缩失败
        if (FileHead.bfType != 19778)
        {
            return 0;
        }
        fin.read((char*)&InfoHead, sizeof(InfoHead)); // 读取信息头
        fin.read((char*)&Color, FileHead.bfOffBits - sizeof(InfoHead) - sizeof(FileHead)); // 读取调色板颜色
        fin.seekg(FileHead.bfOffBits, ios_base::beg); // 定位到像素数据
        ui->progressBar->setValue(500); // 设置进度条
        name += ".bmp"; // 生成解压后的文件名
        // 以二进制方式打开解压后的文件
        ofstream fout(&name[0], ios::binary);
        // 如果文件打开失败则返回0表示解压缩失败
        if (!fout)
        {
            return 0;
        }
        fout.write((char*)&FileHead, sizeof(FileHead)); // 将文件头写入文件
        fout.write((char*)&InfoHead, sizeof(InfoHead)); // 将信息头写入文件
        fout.write((char*)&Color, FileHead.bfOffBits - sizeof(InfoHead) - sizeof(FileHead)); // 将调色板颜色写入文件
        //头文件写入
        ui->progressBar->setValue(1000); // 设置进度条
        vector<unsigned int>CpsData; // 用于存储压缩后的像素数据
        unsigned int temp = 0; // 中间变量
        // 循环读取像素数据并存储到CpsData中
        while (!fin.eof())
        {
            fin.read((char*)&temp, sizeof(temp)); // 这里temp用来暂存数据
            CpsData.push_back(temp);
        }
        CpsData.pop_back(); // 退掉多出来的最后一个，并使中间变量为0
        ui->progressBar->setValue(1500);//设置进度条
        fin.close();
        //开始分析数据
        int count = InfoHead.biWidth*InfoHead.biHeight;
        int track = 0;//用来记录跟踪和写回了几个像素
        int x, y;//个数，位数
        vector<unsigned char>data;
        while (1)
        {
            if (track + 8 < 32)
            {
                x = ((CpsData[0] << track) >> 24) + 1;
                track += 8;
            }
            else if (track + 8 == 32)
            {
                x = ((CpsData[0] << 24) >> 24) + 1;
                track = 0;
                CpsData.erase(CpsData.begin());
            }
            else// track + 8 > 32
            {
                int t = 32 - track;//先存t位
                x = (CpsData[0] << track) >> track;
                track = 8 - t;//再用8-t位
                CpsData.erase(CpsData.begin());
                x = ((x << track) | (CpsData[0] >> (32 - track))) + 1;
            }
            //8位读完
            if (track + 3 < 32)
            {
                y = ((CpsData[0] << track) >> 29) + 1;
                track += 3;
            }
            else if (track + 3 == 32)
            {
                y = ((CpsData[0] << 29) >> 29) + 1;
                track = 0;
                CpsData.erase(CpsData.begin());
            }
            else// track + 3 >32
            {
                int t = 32 - track;//先存t位
                y = (CpsData[0] << track) >> track;
                track = 3 - t;//再用3-t位
                CpsData.erase(CpsData.begin());
                y = ((y << track) | (CpsData[0] >> (32 - track))) + 1;
            }
            //3位读完
            for (int i = 0; i < x; i++)
            {
                if (y + track < 32)
                {
                    temp = ((CpsData[0] << track) >> (32 - y));
                    data.push_back(temp);
                    if (data.size() == count)
                        goto Finsh;
                    track += y;
                }
                else if (y + track == 32)
                {
                    temp = (CpsData[0] << track) >> track;
                    data.push_back(temp);
                    if (data.size() == count)
                        goto Finsh;
                    track = 0;
                    CpsData.erase(CpsData.begin());
                }
                else// y + track > 32
                {
                    int t = 32 - track;//先存t位
                    temp = (CpsData[0] << track) >> track;
                    track = y - t;//再用y-t位
                    CpsData.erase(CpsData.begin());
                    temp = (temp << track) | ((CpsData[0]) >> (32 - track));
                    data.push_back(temp);
                    if (data.size() == count)
                        goto Finsh;
                }
            }
            ui->progressBar->setValue(1500+3500*data.size()/count);//设置进度条
        }
    Finsh:
        vector<unsigned char>Pixel;
        for (int i = 1; i <= InfoHead.biHeight; i++)
        {
            for (int j = 1; j <= InfoHead.biWidth; j++)
            {
                Pixel.push_back(data[0]);
                data.erase(data.begin());
            }
            if (i % 2 != 0)//奇数行
            {
                while (Pixel.size() > 0)
                {
                    fout.write((char*)&Pixel[0], sizeof(Pixel[0]));
                    Pixel.erase(Pixel.begin());
                }
            }
            else //偶数
            {
                while (Pixel.size() > 0)
                {
                    fout.write((char*)&Pixel.back(), sizeof(Pixel.back()));
                    Pixel.pop_back();
                }
            }
            ui->progressBar->setValue(5000+5000*i/InfoHead.biHeight);//设置进度条
        }
        fout.close();
        return 1;
    }
    else return 0;
}

BmpCompress::~BmpCompress()
{
    delete ui;
}

void BmpCompress::on_pushButton_clicked()//压缩浏览文件
{
     QString path = QFileDialog::getOpenFileName(this, tr("bmp灰度图"), ".", tr("*.bmp"));
     ui->CpsName->setPlainText(path);
}

void BmpCompress::on_pushButton_3_clicked()//解压浏览文件
{
    QString path = QFileDialog::getOpenFileName(this, tr("压缩后的dp文件"), ".", tr("*.dp"));
    ui->UnCpsName->setPlainText(path);
}

void BmpCompress::on_pushButton_2_clicked()//压缩
{
    QString path = ui->CpsName->toPlainText();
    string name=path.toStdString();
    ui->progressBar->setValue(0);
    bool IsSucceed=Compress(name,ui);
    if (IsSucceed==1)
    {
        ui->progressBar->setValue(10000);
        QMessageBox::about(NULL, "提示", "压缩成功");
    }
    else
    {
        QMessageBox::about(NULL, "错误", "文件异常");

    }
    ui->progressBar->setValue(0);
}

void BmpCompress::on_pushButton_4_clicked()//解压
{
    QString path = ui->UnCpsName->toPlainText();
    string name=path.toStdString();
    ui->progressBar->setValue(0);
    bool IsSucceed=UnCompress(name,ui);
    if (IsSucceed==1)
    {
        ui->progressBar->setValue(10000);
        QMessageBox::about(NULL, "提示", "解压成功");
    }
    else
    {
        QMessageBox::about(NULL, "错误", "文件异常");
    }
    ui->progressBar->setValue(0);
}
