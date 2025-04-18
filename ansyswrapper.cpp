#include <QtGlobal>
#include <QDateTime>
#include <QFile>
#include <QProcess>
#include <cfloat>
#include <random>

#include "ansyswrapper.h"

#ifndef ANSYSWRAPPER_CPP_INCLUDED
#define ANSYSWRAPPER_CPP_INCLUDED

#define ANS_HI_VER 300	// ANSYS 30.0
#define ANS_LOW_VER 50	// ANSYS 5.0
#define BUFLEN 255

#define ANSLIC "ansys"

#define INPUTFILE "input.dat"
#define OUTPUTFILE "output.dat"

ansysWrapper::ansysWrapper(bool isBatch)
{
    m_kpid = 1;
    m_apdl = "";
    prep7();
    m_isBatch = isBatch;
    tempDir.setAutoRemove(true);
    m_projectPath = tempDir.path();
    m_projectPath = QDir::toNativeSeparators(m_projectPath);

    QDateTime date = QDateTime::currentDateTime();
    m_jobName = QString("MatViz3D-%1-%2-%3-%4")
                    .arg(date.date().year())
                    .arg(date.date().month())
                    .arg(date.date().day())
                    .arg(date.time().second());

    findNp();
    findPathVersion();
    defaultArgs();
    std::random_device myRandomDevice;
    this->seed = myRandomDevice();
}

void ansysWrapper::setWorkingDirectory(QString path)
{
    #if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        tempDir = QTemporaryDir(path);
    #endif
    
    if (!tempDir.isValid()) {
        qCritical() << "Failed to create temporary directory at path:" << path;
        return;
    }
    m_projectPath = tempDir.path();
    m_projectPath = QDir::toNativeSeparators(m_projectPath);
    qDebug() << "Woring directory is set to: " << m_projectPath;
    defaultArgs();
}

void ansysWrapper::setSeed(unsigned int seed)
{
    this->seed = seed;
}

bool ansysWrapper::clear_temp_data()
{
    bool res = tempDir.remove();
    qDebug() << "Ansys tmp dir has been removed :" << res;
    return res;
}

bool ansysWrapper::run()
{
    return this->run(m_apdl);
}

bool ansysWrapper::run(QString apdl)
{
    QString fileName;

    if (m_isBatch)
        fileName = m_projectPath + "/" + QString::fromLatin1(INPUTFILE);
    else
    {
        QString ansVersionStr;
        ansVersionStr = QString::number(m_ansVersion);
        fileName = m_projectPath + "/start.ans";
    }

    qDebug() << "temp dir path" << m_projectPath;
    if (!tempDir.isValid())
    {
        qDebug() <<"Temp dir error: " << tempDir.errorString();
        return false;
    }

    QFile f(fileName);
    if(!f.open(QFile::WriteOnly|QFile::Text))
    {
        qDebug() << "Could not open the file for writing:" << fileName;
        return false;
    }
    f.write(apdl.toLatin1());
    f.close();

    qDebug() << "Ansys project path " << m_projectPath;
    qDebug() << "Ansys path and arguments: " << m_arg;
    qDebug() << "Ansys executable " << m_pathToAns;

    QProcess pr;
    pr.setWorkingDirectory(m_projectPath);
    pr.start(m_pathToAns, m_arg);

    QString monitorFilePath = m_projectPath + "/" + m_jobName + ".mntr";
    QFile monitorFile(monitorFilePath);

    while (!pr.waitForFinished(1000)) // Check every second
    {
        if (monitorFile.exists() && monitorFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&monitorFile);
            QString lastLine;
            while (!in.atEnd())
            {
                lastLine = in.readLine();
            }
            monitorFile.close();

            if (!lastLine.isEmpty())
            {
                qDebug().noquote() << "ANSYS Progress: " << lastLine;
            }
        }
    }

    qDebug().noquote() << "Ansys process output:\n" + pr.readAll();
    qDebug() << "Ansys process finished with code: " << pr.exitCode() << " Status: " << exitCodeToText(pr.exitCode());

    QFile ans_output(tempDir.filePath(m_jobName+".err"));
    if(ans_output.open(QFile::ReadOnly | QFile::Text))
    {
        qDebug().noquote() << ans_output.readAll();
        ans_output.close();
    }
    bool status = pr.exitCode() == 0 || pr.exitCode() == 8;
    return status;
}

QString ansysWrapper::exitCodeToText(int retcode)
{
    QMap<int, QString> exitcodes;

    exitcodes[0] = "Normal Exit";
    exitcodes[1] = "Stack Error";
    exitcodes[2] = "Stack Error";
    exitcodes[3] = "Stack Error";
    exitcodes[4] = "Stack Error";
    exitcodes[5] = "Command Line Argument Error";
    exitcodes[6] = "Accounting File Error";
    exitcodes[7] = "Auth File Verification Error";
    exitcodes[8] = "Error in ANSYS or End-of-run";
    exitcodes[11] = "User Routine Error";
    exitcodes[12] = "Macro STOP Command";
    exitcodes[14] = "XOX Error";
    exitcodes[15] = "Fatal Error";
    exitcodes[16] = "Possible Full Disk";
    exitcodes[17] = "Possible Corrupted or Missing File";
    exitcodes[18] = "Possible Corrupted DB File";
    exitcodes[21] = "Authorized Code Section Entered";
    exitcodes[25] = "Unable to Open X11 Server";
    exitcodes[30] = "Quit Signal";
    exitcodes[31] = "Failure to Get Signal";
    exitcodes[32] = "System-dependent Error";

    if (retcode > 32)
        exitcodes[retcode] = "Unknown Error. Check for *.lock files in working directory and delete it";

    return exitcodes[retcode];
}

void ansysWrapper::defaultArgs()
{
    //-g -p ane3fl -np 2 -dir "E:\ans_proj\temp" -j "MYJOB" -s noread -l en-us -t -d win32
    m_arg.clear();
    if (m_isBatch)
    {
        //m_arg = QString::asprintf(" -b -p %s -np %u -dir \"%s\" -j \"%s\" -s noread -i %s -o %s -d win32",
        //                           ANSLIC, m_np, m_projectPath.toLatin1().data() , m_jobName.toLatin1().data(), INPUTFILE, OUTPUTFILE );
        m_arg <<
            QString("-b") <<
            QString("-p") << ANSLIC <<
            QString("-np") << QString::number(m_np) <<
            QString("-dir") << m_projectPath <<
            QString("-j") << m_jobName <<
            QString("-i") << INPUTFILE  <<
            QString("-o") << OUTPUTFILE  <<
            QString("-d") << "win32";
    }
    else
    {
        //m_arg = QString::asprintf(" -g -p %s -np %u -dir \"%s\" -j \"%s\" -s read -d win32",
        //ANSLIC, m_np, m_projectPath.toLatin1().data() , m_jobName.toLatin1().data());

        m_arg <<
            QString("-g") <<
            QString("-p") << ANSLIC <<
            QString("-np") << QString::number(m_np) <<
            QString("-dir") << m_projectPath <<
            QString("-j") << m_jobName <<
            QString("-s") << "read" <<
            QString("-d") << "win32";
    }
}


void ansysWrapper::createFEfromArray(int32_t*** voxels, short int numCubes, int numSeeds, bool is_random_orientation)
{
    for (int i = 0; i < numSeeds + 1; i++)
        this->createLocalCS(is_random_orientation);

    static const float node_coordinates[21][3] =
        {
            {-999, -999, -999},
            {0.0, 0.0, 0.0}, // 1
            {1.0, 0.0, 0.0}, // 2
            {1.0, 1.0, 0.0}, // 3
            {0.0, 1.0, 0.0}, // 4

            {0.0, 0.0, 1.0}, // 5
            {1.0, 0.0, 1.0}, // 6
            {1.0, 1.0, 1.0}, // 7
            {0.0, 1.0, 1.0}, // 8

            {0.5, 0.0, 0.0}, // 9
            {1.0, 0.5, 0.0}, // 10
            {0.5, 1.0, 0.0}, // 11
            {0.0, 0.5, 0.0}, // 12

            {0.5, 0.0, 1.0}, // 13
            {1.0, 0.5, 1.0}, // 14
            {0.5, 1.0, 1.0}, // 15
            {0.0, 0.5, 1.0}, // 16

            {0.0, 0.0, 0.5}, // 17
            {1.0, 0.0, 0.5}, // 18
            {1.0, 1.0, 0.5}, // 19
            {0.0, 1.0, 0.5}, // 20
            /*
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 0.0},
            {0.0, 0.5, 0.0},
            {1.0, 1.0, 0.0},
            {0.5, 1.0, 0.0},
            {1.0, 0.0, 0.0},
            {1.0, 0.5, 0.0},
            {0.5, 0.0, 0.0},
            {0.0, 0.0, 1.0},
            {1.0, 0.0, 1.0},
            {0.5, 0.0, 1.0},
            {1.0, 1.0, 1.0},
            {1.0, 0.5, 1.0},
            {0.0, 1.0, 1.0},
            {0.5, 1.0, 1.0},
            {0.0, 0.5, 1.0},
            {0.0, 0.0, 0.5},
            {1.0, 0.0, 0.5},
            {1.0, 1.0, 0.5},
            {0.0, 1.0, 0.5},*/
        };

    n3d::node3d key;
    QVector<int> elemets;
    QVector<int> tmp_elemets(20);
    int node_number = 0;

    int approx_num_nodes = 4 * pow(numCubes,3) + 9 * pow(numCubes,2) + 6 * numCubes + 1; // this make by building regression
    nodes.reserve(approx_num_nodes); // allocate memory for 20*numCubes^3 nodes
    QHash<int, n3d::node3d> reverse_nodes;
    reverse_nodes.reserve(approx_num_nodes); // allocate memory for 20*numCubes^3 nodes
    qDebug() << "Creating nodes";
    for (int i = 0; i < numCubes; i++)
    {
        for (int j = 0; j < numCubes; j++)
            for (int k = 0; k < numCubes; k++)
            {
                if (voxels[i][j][k] == 0) // skip empty cells
                    continue;

                for (int l = 1; l <= 20; l++)
                {
                    key.data[0] = node_coordinates[l][0] + i;
                    key.data[1] = node_coordinates[l][1] + j;
                    key.data[2] = node_coordinates[l][2] + k;
                    int current_node = l - 1;
                    if (nodes.contains(key))
                    {
                        tmp_elemets[l-1] = nodes[key];
                    }
                    else
                    {
                        nodes.insert(key, node_number + current_node);
                        reverse_nodes.insert(node_number + current_node, key);
                        tmp_elemets[l-1] = node_number + current_node;
                    }

                }
                node_number += 20;
                elemets.append(tmp_elemets);
            }
        if (i%3 == 0)
        {
            qDebug() << "Progress: " << i * 100 / numCubes <<"%";
        }
    }

    QTextStream apdl(&m_apdl);


    apdl << "NBLOCK,6,SOLID,"<< Qt::endl;
    apdl <<"(3i9,6e21.13e3)" << Qt::endl ;
    qDebug() << "Adding apdl for nodes";
    for (auto ni = nodes.constBegin(); ni!=nodes.constEnd(); ni++)
    {
        apdl.setFieldWidth(9);
        apdl << ni.value()+1 << 0 << 0;
        apdl.setFieldWidth(21);
        apdl << QString::number(ni.key().data[0], 'E', 13)
             << QString::number(ni.key().data[1], 'E', 13)
             << QString::number(ni.key().data[2], 'E', 13);
        apdl.setFieldWidth(0);
        apdl << Qt::endl;
    }
    apdl <<"N,R5.3,LOC,-1," << Qt::endl;

    qDebug() << "Adding apdl for elements";
    apdl << "EBLOCK,19,SOLID" << Qt::endl;
    apdl << "(19i10)" << Qt::endl;
    size_t el_size = elemets.size();

    for (size_t ei = 0; ei < el_size; ei+=20)
    {
        key = reverse_nodes.value(elemets[ei]);
        int kx = (int)key[0], ky = (int)key[1], kz = (int)key[2];
        int mat_id = 1;
        int el_id = 1;
        int real_const = 1;
        int sec_id = 1;
        int coord_sys_id = voxels[kx][ky][kz] + 11; // 11 - first ansys user-def coord sys
        int bd_flag = 0;
        int sld_ref = 0;
        int el_shape = 0;
        int el_num_nodes = 20;
        int exclude_key = 0;
        int el_number = ei / el_num_nodes + 1;

        apdl.setFieldWidth(10);
        apdl << mat_id << el_id << real_const << sec_id << coord_sys_id << bd_flag << sld_ref
             << el_shape << el_num_nodes << exclude_key << el_number;
        for (size_t j = ei; j < ei+20; j++)
        {
            apdl << elemets[j] + 1;
            if (j - ei == 7)
            {
                apdl.setFieldWidth(0);
                apdl << Qt::endl;
                apdl.setFieldWidth(10);
            }
        }
        apdl.setFieldWidth(0);
        apdl << Qt::endl;

        if (ei%(el_size/100) == 0)
            qDebug() << "Progress: " << ei * 100 / el_size <<"%";

    }
    qDebug() << "Done";
    apdl << -1 << Qt::endl; // indicate end of the list
    apdl << Qt::endl;
    apdl << "CSYS, 0"<<Qt::endl; //switch to global coord system
    QString FEM_info = "FE Model info:\n\t nodes:%1\n\t elements:%2";
    qInfo().noquote() << FEM_info.arg(nodes.size()).arg(elemets.size()/20);
}

void ansysWrapper::createFEfromArray8Node(int32_t*** voxels, short int numCubes, int numSeeds, bool is_random_orientation)
{
    for (int i = 0; i < numSeeds + 1; i++)
        this->createLocalCS(is_random_orientation);

    static const float node_coordinates[9][3] =
        {
            {-999, -999, -999},
            {0.0, 0.0, 0.0}, // 1
            {1.0, 0.0, 0.0}, // 2
            {1.0, 1.0, 0.0}, // 3
            {0.0, 1.0, 0.0}, // 4
            {0.0, 0.0, 1.0}, // 5
            {1.0, 0.0, 1.0}, // 6
            {1.0, 1.0, 1.0}, // 7
            {0.0, 1.0, 1.0}  // 8
        };

    n3d::node3d key;
    QVector<int> elements;
    QVector<int> tmp_elements(8);
    int node_number = 0;

    int approx_num_nodes = pow(numCubes + 1, 3); // Estimate for memory allocation
    nodes.reserve(approx_num_nodes);
    QHash<int, n3d::node3d> reverse_nodes;
    reverse_nodes.reserve(approx_num_nodes);

    qDebug() << "Creating nodes for 8-node elements";
    for (int i = 0; i < numCubes; i++)
    {
        for (int j = 0; j < numCubes; j++)
            for (int k = 0; k < numCubes; k++)
            {
                if (voxels[i][j][k] == 0) // Skip empty cells
                    continue;

                for (int l = 1; l <= 8; l++)
                {
                    key.data[0] = node_coordinates[l][0] + i;
                    key.data[1] = node_coordinates[l][1] + j;
                    key.data[2] = node_coordinates[l][2] + k;

                    if (nodes.contains(key))
                    {
                        tmp_elements[l - 1] = nodes[key];
                    }
                    else
                    {
                        nodes.insert(key, node_number);
                        reverse_nodes.insert(node_number, key);
                        tmp_elements[l - 1] = node_number;
                        node_number++;
                    }
                }
                elements.append(tmp_elements);
            }
        if (i % 3 == 0)
        {
            qDebug() << "Progress: " << i * 100 / numCubes << "%";
        }
    }

    QTextStream apdl(&m_apdl);

    // Write nodes
    apdl << "NBLOCK,3,,13" << Qt::endl;
    apdl << "(1i9,3e20.9e3)" << Qt::endl;
    for (auto ni = nodes.constBegin(); ni != nodes.constEnd(); ni++)
    {
        apdl.setFieldWidth(9);
        apdl << ni.value() + 1;
        apdl.setFieldWidth(20);
        apdl << QString::number(ni.key().data[0], 'E', 9)
             << QString::number(ni.key().data[1], 'E', 9)
             << QString::number(ni.key().data[2], 'E', 9);
        apdl.setFieldWidth(0);
        apdl << Qt::endl;
    }
    apdl << -1 << Qt::endl;

    // Write element type
    apdl << "ET,1,185" << Qt::endl;

    // Write elements
    apdl << "EBLOCK,19,SOLID,,2" << Qt::endl;
    apdl << "(19i9)" << Qt::endl;
    size_t el_size = elements.size();

    for (size_t ei = 0; ei < el_size; ei += 8)
    {
        key = reverse_nodes.value(elements[ei]);
        int kx = (int)key[0], ky = (int)key[1], kz = (int)key[2];
        int mat_id = 1;
        int el_id = 1;
        int real_const = 1;
        int sec_id = 1;
        int coord_sys_id = voxels[kx][ky][kz] + 11; // 11 - first ansys user-def coord sys
        int bd_flag = 0;
        int sld_ref = 0;
        int el_shape = 0;
        int el_num_nodes = 8;
        int exclude_key = 0;
        int el_number = ei / el_num_nodes + 1;

        apdl.setFieldWidth(9);
        apdl << mat_id << el_id << real_const << sec_id << coord_sys_id << bd_flag << sld_ref
             << el_shape << el_num_nodes << exclude_key << el_number;
        for (size_t j = ei; j < ei + 8; j++)
        {
            apdl << elements[j] + 1;
        }
        apdl.setFieldWidth(0);
        apdl << Qt::endl;

        if (el_size > 100 && ei % (el_size / 100) == 0)
            qDebug() << "Progress: " << ei * 100 / el_size << "%";
    }
    apdl << -1 << Qt::endl;

    // Switch to global coordinate system
    apdl << "CSYS, 0" << Qt::endl;

    QString FEM_info = "FE Model info:\n\t nodes:%1\n\t elements:%2";
    qInfo().noquote() << FEM_info.arg(nodes.size()).arg(elements.size() / 8);
}

int ansysWrapper::createLocalCS(bool is_random_orientation)
{
    QTextStream apdl(&m_apdl);
    int cs_id = this->m_lcs;
    double eu_angles[3] = {0};
    if (is_random_orientation) // otherwise 0 as angle
        this->generate_random_angles(eu_angles, true);

    this->local_cs.push_back(std::vector<float>(eu_angles, eu_angles + 3));
    qDebug() << "Creating local CS #" << cs_id;
    qDebug() << "    phi1 " << eu_angles[0];
    qDebug() << "    phi "  << eu_angles[1];
    qDebug() << "    phi2 " << eu_angles[2];

    apdl << "LOCAL,"<< this->m_lcs << ", 0, 0, 0, 0,"
         << eu_angles[0] << "," << eu_angles[1] << "," << eu_angles[2]
         << "," << 1 << "," << 1 <<",\n";
    this->m_lcs++;
    return cs_id;
}

void ansysWrapper::applyTensBC(double x1, double y1, double z1,
                               double x2, double y2, double z2,
                               double epsx, double epsy, double epsz)
{
    this->prep7();
    this->clearBC();
    QTextStream apdl(&m_apdl);
    apdl << "!-----Apply Tension BC -------"<< Qt::endl;
    apdl << "NSEL,S,LOC,X," << x1 << Qt::endl;
    apdl << "D, ALL, UX, 0" << Qt::endl;

    apdl << "NSEL,S,LOC,X," << x2 << Qt::endl;
    apdl << "D, ALL, UX," << x2 * epsx << Qt::endl;

    apdl << "NSEL,S,LOC,Y," << y1 << Qt::endl;
    apdl << "D, ALL, UY, 0" << Qt::endl;

    apdl << "NSEL,S,LOC,Y," << y2 << Qt::endl;
    apdl << "D, ALL, UY," << y2 * epsy<< Qt::endl;

    apdl << "NSEL,S,LOC,Z," << z1 << Qt::endl;
    apdl << "D, ALL, UZ, 0" << Qt::endl;
    apdl << "NSEL,S,LOC,Z," << z2 << Qt::endl;
    apdl << "D, ALL, UZ, " << z2*epsz<<Qt::endl;

    apdl << "NSEL,S, , ,all" << Qt::endl;
    apdl << "LSWRITE," << Qt::endl;
}

void ansysWrapper::applyPureShearBC(double x1, double y1, double z1, double x2, double y2, double z2, const QString& shear_plane, double shear_disp)
{
    this->prep7();
    this->clearBC();
    QTextStream apdl(&m_apdl);

    char load_direction_axis;
    double load_dir_min, load_dir_max, load_magnitude;
    char load_sliding_axis;
    double slide_min, slide_max;
    char free_axis;

    if (shear_plane == "XY") {
        load_direction_axis = 'X';
        load_dir_min = x1;
        load_dir_max = x2;
        load_magnitude = x2 * shear_disp;
        load_sliding_axis = 'Y';
        slide_min = y1;
        slide_max = y2;
        free_axis = 'Z';
    } else if (shear_plane == "XZ") {
        load_direction_axis = 'X';
        load_dir_min = x1;
        load_dir_max = x2;
        load_magnitude = x2 * shear_disp;
        load_sliding_axis = 'Z';
        slide_min = z1;
        slide_max = z2;
        free_axis = 'Y';
    } else if (shear_plane == "YZ") {
        load_direction_axis = 'Y';
        load_dir_min = y1;
        load_dir_max = y2;
        load_magnitude = y2 * shear_disp;
        load_sliding_axis = 'Z';
        slide_min = z1;
        slide_max = z2;
        free_axis = 'X';
    } else {
        throw std::invalid_argument("shear_plane should be one of 'XY', 'XZ', or 'YZ'");
    }

    QString load_direction_token = QString("U%1").arg(load_direction_axis);
    QString load_sliding_token = QString("U%1").arg(load_sliding_axis);
    QString free_displ_token = QString("U%1").arg(free_axis);

    apdl << "!-----Apply Pure Shear BC for 3D case -------" << Qt::endl;
    // Bottom face where sliding_axis = min: zero displacement in direction_axis, free in free_axis
    apdl << QString("NSEL,S,LOC,%1,%2").arg(load_sliding_axis).arg(slide_min)  << Qt::endl;
    apdl << QString("D,ALL,%1,0").arg(load_direction_token)  << Qt::endl;
    apdl << QString("D,ALL,%1,0").arg(free_displ_token)  << Qt::endl;
    apdl << "NSEL,ALL\n";

    // Top face where sliding_axis = max: displacement in direction_axis, free in free_axis
    apdl << QString("NSEL,S,LOC,%1,%2").arg(load_sliding_axis).arg(slide_max) << Qt::endl;
    apdl << QString("D,ALL,%1,%2").arg(load_direction_token).arg(load_magnitude) << Qt::endl;
    apdl << QString("D,ALL,%1,0").arg(free_displ_token) << Qt::endl;
    apdl << "NSEL,ALL\n";

    // Left and right faces where direction_axis = min and max: zero displacement in sliding_axis, free in free_axis
    apdl << QString("NSEL,S,LOC,%1,%2").arg(load_direction_axis).arg(load_dir_min) << Qt::endl;
    apdl << QString("NSEL,A,LOC,%1,%2").arg(load_direction_axis).arg(load_dir_max) << Qt::endl;
    apdl << QString("D,ALL,%1,0").arg(load_sliding_token) << Qt::endl;
    apdl << QString("D,ALL,%1,0").arg(free_displ_token) << Qt::endl;
    apdl << "NSEL,ALL" << Qt::endl;
    apdl << "LSWRITE," << Qt::endl;
}


n3d::node3d ansysWrapper::EstimateDisplacement(const n3d::node3d& node,
                                                          double x_origin, double y_origin, double z_origin,
                                                          double epsilon_x, double epsilon_y, double epsilon_z,
                                                          double epsilon_xy, double epsilon_xz, double epsilon_yz)
{
    double nx = node[0], ny = node[1], nz = node[2];
    double local_x = nx - x_origin;
    double local_y = ny - y_origin;
    double local_z = nz - z_origin;

    double u_x = epsilon_x * local_x + epsilon_xy * local_y + epsilon_xz * local_z;
    double u_y = epsilon_y * local_y + epsilon_xy * local_x + epsilon_yz * local_z;
    double u_z = epsilon_z * local_z + epsilon_xz * local_x + epsilon_yz * local_y;

    // return std::make_tuple(u_x, u_y, u_z);
    n3d::node3d displacement;

    displacement.data[0] = u_x;
    displacement.data[1] = u_y;
    displacement.data[2] = u_z;
    return displacement;
}

bool ansysWrapper::IsFaceNode(const n3d::node3d& node,
                              double x1, double y1, double z1,
                              double x2, double y2, double z2)
{
    // node_buffer is the tolerance for being "on the face"
    double node_buffer = 0.25;

    // Extract node coordinates for readability
    double nx = node[0], ny = node[1], nz = node[2];

    // Check proximity to each face of the cube in all three dimensions
    bool closeToXFace = std::abs(nx - x1) <= node_buffer || std::abs(nx - x2) <= node_buffer;
    bool closeToYFace = std::abs(ny - y1) <= node_buffer || std::abs(ny - y2) <= node_buffer;
    bool closeToZFace = std::abs(nz - z1) <= node_buffer || std::abs(nz - z2) <= node_buffer;

    // Node is on a face if it's close to at least one face in any dimension
    bool isOnFace = closeToXFace || closeToYFace || closeToZFace;

    // Additionally, the node must be within the bounds of the other two dimensions
    // to be considered on a face and not outside the cube
    bool withinBoundsX = nx >= std::min(x1, x2) && nx <= std::max(x1, x2);
    bool withinBoundsY = ny >= std::min(y1, y2) && ny <= std::max(y1, y2);
    bool withinBoundsZ = nz >= std::min(z1, z2) && nz <= std::max(z1, z2);

    return isOnFace && withinBoundsX && withinBoundsY && withinBoundsZ;
}

void ansysWrapper::applyComplexLoads(double x1, double y1, double z1,
                                     double x2, double y2, double z2,
                                     double eps_x, double eps_y, double eps_z,
                                     double eps_xy, double eps_xz, double eps_yz)
{
    this->prep7();
    this->clearBC();
    QTextStream apdl(&m_apdl);
    n3d::node3d displacement;

    std::vector<float> eps_vec = {static_cast<float>(eps_x),
                                  static_cast<float>(eps_y),
                                  static_cast<float>(eps_z),
                                  static_cast<float>(eps_xy),
                                  static_cast<float>(eps_yz),
                                  static_cast<float>(eps_xz)};

    this->eps_as_loading.push_back(eps_vec);

    apdl << "!-----Apply BC for 3D case -------" << Qt::endl;
    for (auto ni = nodes.begin(); ni!=nodes.end(); ni++)
    {
        bool is_on_face = ansysWrapper::IsFaceNode(ni.key(), x1, y1, z1, x2, y2, z2);
        int node_id = ni.value() + 1;

        if (is_on_face)
        {
            displacement = EstimateDisplacement(ni.key(), x1, y1, z1, eps_x, eps_y, eps_z, eps_xy, eps_xz, eps_yz);
            apdl << "D, "<< node_id <<", UX, " << displacement[0] << Qt::endl;
            apdl << "D, "<< node_id <<", UY, " << displacement[1] << Qt::endl;
            apdl << "D, "<< node_id <<", UZ, " << displacement[2] << Qt::endl;
        }

    }
    apdl << "!-----END Apply BC for 3D case -------" << Qt::endl;
    apdl << "NSEL,S, , ,all" << Qt::endl;
    apdl << "LSWRITE," << Qt::endl;

    // qDebug() << "nodes size:" << nodes.size();

}


void ansysWrapper::prep7()
{
    QTextStream(&m_apdl) << "FINISH" << Qt::endl;
    QTextStream(&m_apdl) << "/prep7" << Qt::endl;
}

void ansysWrapper::clearBC()
{
    QTextStream(&m_apdl) << "LSCLEAR,ALL" << Qt::endl;
}

void ansysWrapper::findNp()
{
    bool ok;
    m_np = qEnvironmentVariableIntValue("NUMBER_OF_PROCESSORS", &ok);
    qDebug() << "Deteced NUMBER_OF_PROCESSORS = " << m_np;
    if (ok == false)
    {
        m_np = 1;
        qDebug() << "Error due to NUMBER_OF_PROCESSORS detection \n";
        qDebug() << "set NUMBER_OF_PROCESSORS = " << m_np;
    }
}

void ansysWrapper::findPathVersion()
{
    QString env;
    m_ansVersion = -1;
    for (int i = ANS_HI_VER; i >= ANS_LOW_VER; i--)
    {
        QString envName = "ANSYS" + QString::number(i) + "_DIR";
        bool ok = qEnvironmentVariableIsSet(envName.toLatin1().data());
        if (ok)
        {
            env = qEnvironmentVariable(envName.toLatin1().data());
            qDebug() << "Ansys is found";
            qDebug() << "\tAnsys version: " << i << "\n" <<
                "\tAnsys path: " << env;
            m_ansVersion = i;
            break;
        }
    }
    if (m_ansVersion == -1)
    {
        qCritical() << "Ansys not found";
        std::terminate();
        return;
    }
    assert(m_ansVersion != -1);
    env += QString(QDir::separator()) + "bin" + QString(QDir::separator());
    m_pathToAns = env;

    env = qEnvironmentVariable("ANSYS_SYSDIR");

    m_pathToAns += env;
    m_pathToAns += QString(QDir::separator()) + "ansys" + QString::number(m_ansVersion);
    #if (defined (_WIN32) || defined (_WIN64))
        m_pathToAns += ".exe";
    #endif
}

int ansysWrapper::kp(double x, double y)
{
    m_apdl += QString::asprintf("k, %d, %g, %g\n", m_kpid, x, y);
    return m_kpid++;
}

int ansysWrapper::spline(std::vector<int> kps, int left_boundary, int right_boundary)
{
    if (kps.size() < 2) return -1;

    m_apdl += QString::asprintf("FLST,3,%zu,3\n", kps.size());

    for (size_t i = 0; i < kps.size(); i++)
    {
        m_apdl += QString::asprintf("FITEM,3,%d\n", kps[i]);
    }

    QString lbstr, rbstr;
    switch (left_boundary)
    {
    default:
    case 0: lbstr = ",,,";     break;
    case 1: lbstr = "-1,0,0,"; break;
    case 2: lbstr = "0,1,0,";  break;
    }

    switch (right_boundary)
    {
    default:
    case 0: rbstr = ",,,";    break;
    case 1: rbstr = "1,0,0,"; break;
    case 2: rbstr = "0,1,0,"; break;
    }

    //m_apdl += "BSPLIN, ALL,,,,,," + lbstr + rbstr +  "\n";
    m_apdl += "BSPLIN, ,P51X,,,,," + lbstr + rbstr +  "\n";
    //   m_apdl += "ALLSEL,ALL\n";
    return 1;
}

int ansysWrapper::spline(std::vector<double> x, std::vector<double> y, int left_boundary, int right_boundary)
{
    std::vector<int> kps;

    for (size_t i = 0; i < x.size(); i++)
    {
        kps.push_back(this->kp(x[i], y[i]));
    }

    this->spline(kps, left_boundary, right_boundary);
    return 1;
}

void ansysWrapper::setMaterial(double E, double nu, double rho = 0)
{
    m_apdl += "MPTEMP,,,,,,,,\n";
    m_apdl += "MPTEMP,1,0\n";
    m_apdl += QString::asprintf("MPDATA,EX,1,,%g \nMPDATA,PRXY,1,,%g \nMPDATA,DENS,1,,%g \n", E, nu, rho);

}

void ansysWrapper::setAnisoMaterial(double c11, double c12, double c13, double c22, double c23, double c33, double c44, double c55, double c66)
{
    m_apdl += QString::asprintf(R"(
        !*
        TB,ANEL,1,1,21,0
        TBTEMP,0
        TBDATA,, %g, %g, %g, 0, 0, 0
        TBDATA,, %g, %g, 0, 0, 0, %g
        TBDATA,, 0, 0, 0, %g, 0, 0
        TBDATA,, %g, 0, %g,,,
        )", c11, c12, c13, c22, c23, c33, c44, c55, c66);
}

void ansysWrapper::setSectionASEC(double area, double Ix, double r_out)
{
    m_apdl += "SECTYPE,   1, BEAM, ASEC, , 0 \n";
    m_apdl += "SECOFFSET, CENT \n";
    m_apdl += QString::asprintf("SECDATA, %g, %g, 0, %g, 0,  %g, 0, 0, 0, 0, %g, %g  \n", area, Ix, Ix, Ix*2, r_out, r_out);
}

void ansysWrapper::setSectionCTUBE(double r_Out, double r_In)
{
    m_apdl += "SECTYPE,   1, BEAM, CTUBE, , 0  \n";
    m_apdl += "SECOFFSET, CENT \n";
    m_apdl += QString::asprintf("SECDATA, %g, %g, 20  \n", r_In, r_Out);
}

void ansysWrapper::setAccelGlobal(double gx, double gy, double gz)
{
    m_apdl += QString::asprintf("ACEL, %g, %g, %g \n", gx, gy, gz);
}

void ansysWrapper::DKAll(int kp_num)
{
    m_apdl += QString::asprintf("DK, %d, , , ,0, ALL \n", kp_num);
}

void ansysWrapper::setLDiv(int ndiv)
{
    m_apdl += QString::asprintf("LESIZE, ALL, , , %d, , , , ,1   \n", ndiv);
}

QString ansysWrapper::mergeVector(QString prefix, std::vector<double> vec)
{
    QString retVal;
    const size_t vec_div_size = 6;
    for (size_t i = 0; i < vec.size(); i += vec_div_size)
    {
        QString data = prefix;
        for (size_t j = i; j < i + vec_div_size && j < vec.size(); j++)
        {
            data += ", " + QString::number(vec[j]);
        }
        retVal += data + "\n";
    }
    return retVal;
}

void ansysWrapper::setSeismicSpectrum(std::vector<double> freq, std::vector<double> ax, std::vector<double> ay, std::vector<double> az)
{
    if (!freq.size()) return;

    //m_apdl += "FINISH  \n/SOLU   \nANTYPE,MODAL \nMODOPT,LANB,10 \nMXPAND,ALL,0,400,YES \nSOLVE \nFINISH \n/SOLU  \n";

    m_apdl += "FINISH \n";
    m_apdl += "/SOLU \n";
    m_apdl += "ANTYPE,MODAL \n";
    m_apdl += "MODOPT,LANB,100 \n";
    m_apdl += "MXPAND,ALL,0,400,YES \n";
    m_apdl += "SOLVE \n";
    m_apdl += "FINISH \n";
    m_apdl += "/SOLU  \n";
    m_apdl += "\n";
    m_apdl += "\n";
    m_apdl += "\n";

    m_apdl +="antype,spectr \nspopt, sprs, 10, 1, 0\n";


    m_apdl += "SED,1,0,0 \nSVTYPE,2\n"; // X - direction
    m_apdl += mergeVector("freq",freq);
    m_apdl += mergeVector("sv,",ax);
    m_apdl +="srss,0.0,disp \nsolve\nfreq \n";

    m_apdl += "SED,0,1,0 \n SVTYPE,2\n"; // Y - direction
    m_apdl += mergeVector("freq",freq);
    m_apdl += mergeVector("sv,",ay);
    m_apdl +="srss,0.0,disp \nsolve \nfreq\n";

    m_apdl += "SED,0,0,1 \nSVTYPE,2\n"; // Z - direction
    m_apdl += mergeVector("freq",freq);
    m_apdl += mergeVector("sv,",az);
    m_apdl +="srss,0.0,disp \nsolve \nfreq\n";


    m_apdl +="FINISH \n/POST1\n";
    m_apdl +="/POST1\n";
    m_apdl +="SAVE\n";

    m_apdl +="/INPUT,'" + m_jobName + "','mcom'\n";
    m_apdl +="LCDEF,88,,,\n";
    m_apdl +="LCWRITE,88,,,\n";

    m_apdl +="LCZERO\n";
    m_apdl +="LCDEF,99,,,\n";
    m_apdl +="LCASE,88,\n";
    m_apdl +="LCOPER,ADD,77,,,\n";
    m_apdl +="LCWRITE,99,,,\n";

    m_apdl +="*ABBR, STAT_NUE, LCASE, 77\n";
    m_apdl +="*ABBR, SEIS_MRZ, LCASE, 88\n";
    m_apdl +="*ABBR, MRZ_NUE, LCASE, 99\n";

    m_apdl +="SAVE\n";

    QString apdl = R"(
SET,FIRST
*get,fr1,active,0,set,freq
SET,next
*get,fr2,active,0,set,freq
SET,next
*get,fr3,active,0,set,freq
eplot
lcase,77

/SHRINK,0
/ESHAPE,1.0
/EFACET,1
/RATIO,1,1,1
/CFORMAT,32,0
/REPLOT

/SHOW,JPEG,,0
/SHRINK,0
/ESHAPE,1.0
/EFACET,1
/RATIO,1,1,1
/CFORMAT,32,0
/REPLOT
PLESOL, S,EQV, 0,1.0
*GET, static_stress, PLNSOL, 0, MAX
/SHOW,CLOSE

/SHOW,JPEG,,0
PLNSOL, U, SUM, 0,1.0
*GET, static_displ, PLNSOL, 0, MAX
/SHOW,CLOSE

lcase,88

/SHOW,JPEG,,0
/SHRINK,0
/ESHAPE,1.0
/EFACET,1
/RATIO,1,1,1
/CFORMAT,32,0
/REPLOT
PLESOL, S,EQV, 0,1.0
*GET, seism_stress, PLNSOL, 0, MAX
/SHOW,CLOSE

/SHOW,JPEG,,0
PLNSOL, U, SUM, 0,1.0
*GET, seism_displ, PLNSOL, 0, MAX
/SHOW,CLOSE

lcase, 99

/SHOW,JPEG,,0
/SHRINK,0
/ESHAPE,1.0
/EFACET,1
/RATIO,1,1,1
/CFORMAT,32,0
/REPLOT
PLESOL, S,EQV, 0,1.0
*GET, static_seism_stress, PLNSOL, 0, MAX
/SHOW,CLOSE

/SHOW,JPEG,,0
PLNSOL, U, SUM, 0,1.0
*GET, static_seism_displ, PLNSOL, 0, MAX
/SHOW,CLOSE

*CFOPEN, result, csv, , APPEND
*VWRITE, fr1, fr2, fr3, static_displ, seism_displ, static_seism_displ, static_stress, seism_stress, static_seism_stress
%G;%G;%G;%G;%G;%G;%G;%G;%G

*CFCLOS
               )";

    m_apdl += apdl;

}

void ansysWrapper::setElem(bool linear)
{
    if (linear)
        m_apdl += "ET,1,BEAM188 \n";
    else
        m_apdl += "ET,1,BEAM189 \n";

    m_apdl +="KEYOPT,1,1,1 \n";
}

void ansysWrapper::setElemByNum(int num)
{
    m_apdl += "ET,1," + QString::number(num) + "\n";
}

void ansysWrapper::mesh()
{
    m_apdl += "LMESH,       ALL \n";
}

QString ansysWrapper::getAnsExec()
{
    return m_pathToAns;
}

void ansysWrapper::setAnsExec(QString exec)
{
    m_pathToAns = exec;
}

QString ansysWrapper::getJobName()
{
    return m_jobName;
}

void ansysWrapper::setJobName(QString jobName)
{
    m_jobName = jobName;
    this->defaultArgs();
}

QString ansysWrapper::getProjectPath()
{
    return m_projectPath;
}

void ansysWrapper::setProjectPath(QString path)
{
    m_projectPath = path;
    this->defaultArgs();
}

void ansysWrapper::clear()
{
    m_apdl = "/prep7 \n";
    m_kpid = 1;
}

void ansysWrapper::solve()
{
    m_apdl += "FINISH\n";
    m_apdl += "/SOL\n";
    m_apdl += "SOLVE\n";
    m_apdl += "SAVE\n";
    m_apdl += "FINISH\n";
    m_apdl += "/POST1\n";
    m_apdl += "\n";
    m_apdl += "\n";
}
void ansysWrapper::solveLS(int start, int end)
{
    m_apdl += "FINISH\n";
    m_apdl += "/SOL\n";
    m_apdl += "LSSOLVE," +QString::number(start) + "," +QString::number(end) + ",";
    m_apdl += "SAVE\n";
    m_apdl += "FINISH\n";
    m_apdl += "/POST1\n";
    m_apdl += "\n";
    m_apdl += "\n";
}

//LSSOLVE,1,3,1,
void ansysWrapper::setNP(int np)
{
    m_np = np;
    defaultArgs();
}

/**
 * @brief Generates uniformly distributed random orientations (Euler angles).
 *
 * This function generates uniformly distributed random orientations in SO(3)
 * using the method of sampling uniformly distributed quaternions. It then
 * converts the quaternion to Bunge ZXZ Euler angles (phi1, Phi, phi2).
 *
 * @param angl Pointer to a double array of size 3 where the calculated
 * Euler angles (phi1, Phi, phi2) in radians will be stored.
 * @param in_deg If true, converts the output angles to degrees.
 * @param epsilon Tolerance for checking singularity conditions (gimbal lock).
 */
void ansysWrapper::generate_random_angles(double *angl, bool in_deg, double epsilon)
{
    if (!angl)
        return;

    // Use the existing random engine seeded in the constructor
    static std::mt19937 re(this->seed);
    // Use a normal distribution for generating quaternion components
    std::normal_distribution<double> norm_dist(0.0, 1.0);

    // 1. Generate 4 random numbers from standard normal distribution
    double q0 = norm_dist(re);
    double q1 = norm_dist(re);
    double q2 = norm_dist(re);
    double q3 = norm_dist(re);

    // 2. Normalize to get a unit quaternion (w, x, y, z)
    double norm = std::sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    // Prevent division by zero, though highly unlikely with normal distribution
    if (norm < epsilon) {
        // Return a default orientation (e.g., identity) in case of issue
        angl[0] = 0.0;
        angl[1] = 0.0;
        angl[2] = 0.0;
        return;
    }
    double w = q0 / norm;
    double x = q1 / norm;
    double y = q2 / norm;
    double z = q3 / norm;

    // 3. Convert quaternion (w, x, y, z) to Bunge ZXZ Euler angles (phi1, Phi, phi2)
    // Calculate necessary rotation matrix elements from quaternion
    double R11 = 1.0 - 2.0 * (y*y + z*z); // w*w + x*x - y*y - z*z
    double R12 = 2.0 * (x*y - w*z);
    //double R13 = 2.0 * (x*z + w*y); // Needed for phi2
    //double R23 = 2.0 * (y*z - w*x); // Needed for phi2
    //double R31 = 2.0 * (x*z - w*y); // Needed for phi1
    //double R32 = 2.0 * (y*z + w*x); // Needed for phi1
    double R33 = 1.0 - 2.0 * (x*x + y*y); // w*w - x*x - y*y + z*z

    double phi1, Phi, phi2;

    // Avoid numerical issues near poles (Phi=0 or Phi=pi)
    // Clamp R33 value to ensure it's within acos domain [-1, 1]
    if (R33 > 1.0) R33 = 1.0;
    if (R33 < -1.0) R33 = -1.0;

    Phi = std::acos(R33); // Phi is in [0, pi]

    // Check for singularity (gimbal lock)
    // sin(Phi) is close to 0 if Phi is close to 0 or pi
    if (std::fabs(Phi) < epsilon || std::fabs(Phi - M_PI) < epsilon)
    {
        // At singularity, phi1 + phi2 (if Phi=0) or phi1 - phi2 (if Phi=pi) is well-defined.
        // Conventionally set phi2 = 0 and calculate phi1.
        // phi1 = atan2(R12, R11) works for both Phi=0 and Phi=pi cases here
        phi1 = std::atan2(R12, R11);
        phi2 = 0.0;
    }
    else
    {
        // Calculate elements needed for non-singular case
        double R13 = 2.0 * (x*z + w*y);
        double R23 = 2.0 * (y*z - w*x);
        double R31 = 2.0 * (x*z - w*y);
        double R32 = 2.0 * (y*z + w*x);

        // General case
        phi1 = std::atan2(R31, -R32); // atan2(R[3][1], -R[3][2])
        phi2 = std::atan2(R13, R23);  // atan2(R[1][3], R[2][3])
    }

    // Ensure angles are in the typical range [0, 2*pi) if desired, atan2 gives [-pi, pi]
    // Bunge convention often uses [0, 2pi) for phi1, [0, pi] for Phi, [0, 2pi) for phi2
    // The ranges from acos/atan2 are naturally: Phi in [0, pi], phi1/phi2 in [-pi, pi]
    // If a specific range is needed (e.g. [0, 2pi)), adjust here:
    // if (phi1 < 0) phi1 += 2.0 * M_PI;
    // if (phi2 < 0) phi2 += 2.0 * M_PI;
    // For now, leave them in [-pi, pi] as APDL LOCAL command handles angles correctly.

    angl[0] = phi1;
    angl[1] = Phi;
    angl[2] = phi2;

    // Convert to degrees if requested
    if (in_deg)
    {
        angl[0] *= 180.0 / M_PI;
        angl[1] *= 180.0 / M_PI;
        angl[2] *= 180.0 / M_PI;
    }
}


void ansysWrapper::load_loadstep(int num)
{
    const int num_columns = 19; // ID;X;Y;Z;UX;UY;UZ;SX;SY;SZ;SXY;SYZ;SXZ;EpsX;EpsY;EpsZ;EpsXY;EpsYZ;EpsXZ
    QString path_to_file = tempDir.filePath(QString("ls_")+QString::number(num)+".csv");
    QFile file(path_to_file);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical() << "Can not open file to read results. Path to file: " << path_to_file;
        return;
    }
    int i = 0;
    QByteArray parts[num_columns]; // ID;X;Y;Z;UX;UY;UZ;SX;SY;SZ;SXY;SYZ;SXZ;EpsX;EpsY;EpsZ;EpsXY;EpsYZ;EpsXZ

    this->loadstep_results.clear();

    while (!file.atEnd()) {
        i++;
        QByteArray line = file.readLine();
        if (i == 1) //skip header line
            continue;

        this->loadstep_results.push_back(std::vector<float>(num_columns));
        for (int j = 0; j < num_columns; j++)
        {
            parts[j] = line.sliced(j*17, 16).trimmed();
            this->loadstep_results[i-2][j] = parts[j].toFloat();
        }
    }
    this->loadstep_results_avg.clear();
    this->loadstep_results_avg.resize(num_columns);

    this->loadstep_results_max.clear();
    this->loadstep_results_max.resize(num_columns);
    std::fill(this->loadstep_results_max.begin(), this->loadstep_results_max.end(), -FLT_MAX);

    this->loadstep_results_min.clear();
    this->loadstep_results_min.resize(num_columns);
    std::fill(this->loadstep_results_min.begin(), this->loadstep_results_min.end(), FLT_MAX);

    for (size_t i = 0; i < this->loadstep_results.size(); i++)
    {
        for (int j = 0; j < num_columns; j++)
        {
            this->loadstep_results_avg[j] += this->loadstep_results[i][j];
            this->loadstep_results_min[j] = std::min(this->loadstep_results_min[j], this->loadstep_results[i][j]);
            this->loadstep_results_max[j] = std::max(this->loadstep_results_max[j], this->loadstep_results[i][j]);
        }
        n3d::node3d key;
        key.data[0] = this->loadstep_results[i][X];
        key.data[1] = this->loadstep_results[i][Y];
        key.data[2] = this->loadstep_results[i][Z];
        int line_id = i;
        this->result_nodes.insert(key, line_id);

    }
    for (int j = 0; j < num_columns; j++)
    {
        this->loadstep_results_avg[j] /= (float)this->loadstep_results.size();
    }
    auto &avg = this->loadstep_results_avg;
    auto &max = this->loadstep_results_max;
    auto &min = this->loadstep_results_min;
    auto &load = this->eps_as_loading[num - 1];
    qDebug() << QString("------ LS NUM %1------- ").arg(num);
    qDebug() << "AVG S tensor: ";
    qDebug() << "  "<< avg[SX]  << avg[SXY] << avg[SXZ];
    qDebug() << "  "<< avg[SXY] << avg[SY]  << avg[SYZ];
    qDebug() << "  "<< avg[SXZ] << avg[SYZ] << avg[SZ] << "\n";


    qDebug() << "AVG EPS tensor: ";
    qDebug() << "  "<< avg[EpsX]  << avg[EpsXY] << avg[EpsXZ];
    qDebug() << "  "<< avg[EpsXY] << avg[EpsY]  << avg[EpsYZ];
    qDebug() << "  "<< avg[EpsXZ] << avg[EpsYZ] << avg[EpsZ] << "\n";


    qDebug() << "AVG EPS Load tensor: ";
    qDebug() << "  "<< load[0] << load[3] << load[5];
    qDebug() << "  "<< load[3] << load[1] << load[4];
    qDebug() << "  "<< load[5] << load[4] << load[2] << "\n";


    auto eps_err = [](float e1, float e2) { return std::round(e2 ? (e2 - e1) / e2 * 10000 : 0) / 100; };
    qDebug() << "AVG EPS Error tensor, %: ";
    qDebug() << "  "<< eps_err(avg[EpsX],    load[0]) << eps_err(avg[EpsXY]/2, load[3]) << eps_err(avg[EpsXZ]/2, load[5]);
    qDebug() << "  "<< eps_err(avg[EpsXY]/2, load[3]) << eps_err(avg[EpsY],    load[1]) << eps_err(avg[EpsYZ]/2, load[4]);
    qDebug() << "  "<< eps_err(avg[EpsXZ]/2, load[5]) << eps_err(avg[EpsYZ]/2, load[4]) << eps_err(avg[EpsZ],    load[2]) << "\n";



    // qDebug() << "MAX S tensor: ";
    // qDebug() << "  "<< max[SX]  << max[SXY] << max[SXZ];
    // qDebug() << "  "<< max[SXY] << max[SY]  << max[SYZ];
    // qDebug() << "  "<< max[SXZ] << max[SYZ] << max[SZ];
    // qDebug() << "\n";
    // qDebug() << "MIN S tensor: ";
    // qDebug() << "  "<< min[SX]  << min[SXY] << min[SXZ];
    // qDebug() << "  "<< min[SXY] << min[SY]  << min[SYZ];
    // qDebug() << "  "<< min[SXZ] << min[SYZ] << min[SZ];
    // qDebug() << "\n";
}

float ansysWrapper::scaleValue01(float val, int component)
{
    auto &maxVal = this->loadstep_results_max;
    auto &minVal = this->loadstep_results_min;
    // Scale each value in the vector to the range [0, 1]

    if (fabs(maxVal[component] - minVal[component]) > 0)
    {
        return (val - minVal[component]) / (maxVal[component] - minVal[component]);
    }

    return 1; // return 1 for case max == min
}

float ansysWrapper::getValByCoord(float x, float y, float z, int component)
{
    n3d::node3d key;
    key.data[0] = x;
    key.data[1] = y;
    key.data[2] = z;

    return getValByCoord(key, component);
}

float ansysWrapper::getValByCoord(n3d::node3d &key, int component)
{
    if (this->result_nodes.contains(key))
    {
        int line_id = this->result_nodes[key];
        float res = this->loadstep_results[line_id][component];
        return res;
    }
    qDebug() << "Coord not found : "<< key[0] << key[1] << key[2];
    return 0;
}


float ansysWrapper::calc_avg(QVector<float> &x)
{
    if (x.size() > 0)
        return std::accumulate(x.begin(), x.end(), 0.0f) / (float)x.size();

    qWarning() << "Zero sized vector in calc_avg()";
    return 0;
}

void ansysWrapper::saveAll()
{
    auto s = R"(

alls     !this selects everything, good place to start

!
set,first
*GET,numb_sets,ACTIVE,0,SET,NSET !get number of results sets

NSEL,S,S,EQV,,, ,0 !- Select nodes with results

*get,nummax,NODE,,num,max !Max Node id in selected nodes
*get,numnode,NODE,,count !Number of selected nodes
*dim,mask,array,nummax
*vget,mask(1),NODE,,NSEL !mask for selected nodes
*dim,nodal_data_full,array,nummax,18 !array for pressures of all 1-to-nummax nodes
*dim,nodal_data_comp,array,numnode,18 !array for pressures of ONLY selected nodes
*dim,nodeid_full,array,nummax !array for containing NODE ID
*dim,nodeid_comp,array,numnode !array for containing NODE ID for ONLY selected nodes
*vfill,nodeid_full,ramp,1,1 !array from 1-to-nummnode (1,2,3,....)
!
*do,i_set,1,numb_sets,1
    *GET,current_time,ACTIVE,0,SET,TIME
    !Retrieving DATA
    *vmask,mask(1)
    *vget,nodal_data_full(1,1),node,,LOC,X
    *vmask,mask(1)
    *vget,nodal_data_full(1,2),node,,LOC,Y
    *vmask,mask(1)
    *vget,nodal_data_full(1,3),node,,LOC,Z
    *vmask,mask(1)
    *vget,nodal_data_full(1,4),node,,U,X !*VGET,ParR,Entity,ENTNUM,Item1,IT1NUM,Item2,IT2NUM,KLOOP
    *vmask,mask(1)
    *vget,nodal_data_full(1,5),node,,U,Y
    *vmask,mask(1)
    *vget,nodal_data_full(1,6),node,,U,Z
    *vmask,mask(1)
    *vget,nodal_data_full(1,7),node,,S,X
    *vmask,mask(1)
    *vget,nodal_data_full(1,8),node,,S,Y
    *vmask,mask(1)
    *vget,nodal_data_full(1,9),node,,S,Z
    *vmask,mask(1)
    *vget,nodal_data_full(1,10),node,,S,XY
    *vmask,mask(1)
    *vget,nodal_data_full(1,11),node,,S,YZ
    *vmask,mask(1)
    *vget,nodal_data_full(1,12),node,,S,XZ

    *vmask,mask(1)
    *vget,nodal_data_full(1,13),node,,EPTO,X
    *vmask,mask(1)
    *vget,nodal_data_full(1,14),node,,EPTO,Y
    *vmask,mask(1)
    *vget,nodal_data_full(1,15),node,,EPTO,Z
    *vmask,mask(1)
    *vget,nodal_data_full(1,16),node,,EPTO,XY
    *vmask,mask(1)
    *vget,nodal_data_full(1,17),node,,EPTO,YZ
    *vmask,mask(1)
    *vget,nodal_data_full(1,18),node,,EPTO,XZ

!
!COMPRESSING ARRAYS TO ONLY SELECTED NODES for the coordinate locations and displacements:
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,1),COMP,nodal_data_full(1,1)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,2),COMP,nodal_data_full(1,2)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,3),COMP,nodal_data_full(1,3)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,4),COMP,nodal_data_full(1,4)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,5),COMP,nodal_data_full(1,5)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,6),COMP,nodal_data_full(1,6)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,7),COMP,nodal_data_full(1,7)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,8),COMP,nodal_data_full(1,8)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,9),COMP,nodal_data_full(1,9)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,10),COMP,nodal_data_full(1,10)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,11),COMP,nodal_data_full(1,11)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,12),COMP,nodal_data_full(1,12)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,13),COMP,nodal_data_full(1,13)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,14),COMP,nodal_data_full(1,14)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,15),COMP,nodal_data_full(1,15)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,16),COMP,nodal_data_full(1,16)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,17),COMP,nodal_data_full(1,17)
    *vmask,mask(1)
    *vfun,nodal_data_comp(1,18),COMP,nodal_data_full(1,18)
!
!GETTING THE SELECTED NODE ID LIST !only necessary if you also want to keep the node numbers
    *vmask,mask(1)
    *vfun,nodeid_comp(1),COMP,nodeid_full(1)
!
!WRITING TO A FILE: !set the file name as desired.  includes the time step in the file name as a variable

*cfopen,ls_%current_time%,csv

*vwrite,'ID','X','Y','Z','UX','UY','UZ','SX','SY','SZ','SXY','SYZ','SXZ','EpsX','EpsY','EpsZ','EpsXY','EpsYZ','EpsXZ'
%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C

*vwrite,nodeid_comp(1),nodal_data_comp(1,1),nodal_data_comp(1,2),nodal_data_comp(1,3),nodal_data_comp(1,4), nodal_data_comp(1,5), nodal_data_comp(1,6), nodal_data_comp(1,7), nodal_data_comp(1,8), nodal_data_comp(1,9), nodal_data_comp(1,10), nodal_data_comp(1,11), nodal_data_comp(1,12), nodal_data_comp(1,13),  nodal_data_comp(1,14), nodal_data_comp(1,15), nodal_data_comp(1,16), nodal_data_comp(1,17), nodal_data_comp(1,18)
(F16.0";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8)
*cfclos
!(18E16.8)
!%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8;%E16.8
!
set,next !read next set
*ENDDO !end loop
!
!DELETING USED VARIABLES
*del,nodal_data_full,nopr
*del,nodeid_full,nopr
*del,nodal_data_comp,nopr
*del,nodeid_comp,nopr
*del,mask,nopr
*del,nummax,nopr
*del,numnode,nopr
ALLS

                )";

    m_apdl += s;
}

void ansysWrapper::addStrainToBCMacro(double eps_xx, double eps_yy, double eps_zz,
                                      double eps_xy, double eps_xz, double eps_yz, int CubeSize)
{
    this->prep7();
    this->clearBC();

    std::vector<float> eps_vec = {static_cast<float>(eps_xx),
                                  static_cast<float>(eps_yy),
                                  static_cast<float>(eps_zz),
                                  static_cast<float>(eps_xy),
                                  static_cast<float>(eps_yz),
                                  static_cast<float>(eps_xz)};

    this->eps_as_loading.push_back(eps_vec);

    this->m_apdl += "!-----Apply BC for 3D case -------\n";

    this->m_apdl += QString("ApplyStrainAsDisp, %1, %2, %3, %4, %5, %6, %7\n").\
                    arg(eps_xx).arg(eps_yy).arg(eps_zz).arg(eps_xy).arg(eps_yz).arg(eps_xz).arg(CubeSize);

    this->m_apdl += "!-----END Apply BC for 3D case -------\n";
    this->m_apdl += "NSEL,S, , ,all\n";
    this->m_apdl += "LSWRITE,\n";
}

void ansysWrapper::addStrainToBCMacroBlob()
{
    auto s = R"(

*CREATE, ApplyStrainAsDisp, mac
!---------------------------------------------------------------------
! Macro: ApplyStrainAsDisp
! Purpose: Create loading tables and apply displacements based on
!          the given strain components and cube size.
!
! Input:
!   epsxx, epsyy, epszz - Normal strain components
!   epsxy, epsyz, epsxz - Shear strain components
!   cubeSize          - Size of the cube along X, Y, and Z directions
!---------------------------------------------------------------------

epsxx = ARG1
epsyy = ARG2
epszz = ARG3
epsxy = ARG4
epsyz = ARG5
epsxz = ARG6
cubeSize = ARG7

*DEL,loadtableUX

loadtableUX=
*dim,loadtableUX(1),table,2,2,2,x,y,z !-- Define loading table
*taxis, loadtableUX(1), 1, 0, cubeSize !-- Set range [0, cubeSize] for X-axis
*taxis, loadtableUX(1), 2, 0, cubeSize !-- Set range [0, cubeSize] for Y-axis
*taxis, loadtableUX(1), 3, 0, cubeSize !-- Set range [0, cubeSize] for X-axis

loadtableUX(1,1,1) = 0

loadtableUX(2,1,1) = epsxx
loadtableUX(2,2,1) = epsxx + epsxy
loadtableUX(2,2,2) = epsxx + epsxy + epsxz

loadtableUX(1,2,1) = epsxy
loadtableUX(1,2,2) = epsxy + epsxz
loadtableUX(1,1,2) = epsxz

loadtableUX(2,1,2) = epsxx + epsxz


*DEL,loadtableUY
loadtableUY=
*dim,loadtableUY(1),table,2,2,2,x,y,z
*taxis, loadtableUY(1), 1, 0, cubeSize
*taxis, loadtableUY(1), 2, 0, cubeSize
*taxis, loadtableUY(1), 3, 0, cubeSize

loadtableUY(1,1,1) = 0

loadtableUY(2,1,1) = epsxy
loadtableUY(2,2,1) = epsxy + epsyy
loadtableUY(2,2,2) = epsxy + epsyy + epsyz

loadtableUY(1,2,1) = epsyy
loadtableUY(1,2,2) = epsyy + epsyz
loadtableUY(1,1,2) = epsyz

loadtableUY(2,1,2) = epsxy + epsyz


*DEL,loadtableUZ
loadtableUZ=
*dim,loadtableUZ(1),table,2,2,2,x,y,z
*taxis, loadtableUZ(1), 1, 0, cubeSize
*taxis, loadtableUZ(1), 2, 0, cubeSize
*taxis, loadtableUZ(1), 3, 0, cubeSize

loadtableUZ(1,1,1) = 0

loadtableUZ(2,1,1) = epsxz
loadtableUZ(2,2,1) = epsxz + epsyz
loadtableUZ(2,2,2) = epsxz + epsyz + epszz

loadtableUZ(1,2,1) = epsyz
loadtableUZ(1,2,2) = epsyz + epszz
loadtableUZ(1,1,2) = epszz

loadtableUZ(2,1,2) = epsxz + epszz

!--- Selecing nodes on faces
NSEL,S,LOC,X,0
NSEL,A,LOC,X,cubeSize

NSEL,A,LOC,Y,0
NSEL,A,LOC,Y,cubeSize

NSEL,A,LOC,Z,0
NSEL,A,LOC,Z,cubeSize

!- Appling displacement
D,All,UX,%loadtableUX%
D,All,UY,%loadtableUY%
D,All,UZ,%loadtableUZ%

Allsel,all

*END



                )";

    m_apdl += s;
}

#endif // ANSYSWRAPPER_CPP_INCLUDED

