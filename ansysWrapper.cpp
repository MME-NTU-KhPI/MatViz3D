#include <QtGlobal>
#include <QDateTime>
#include <QFile>
#include <QProcess>
#include <random>

#include "ansysWrapper.h"

#ifndef ANSYSWRAPPER_CPP_INCLUDED
#define ANSYSWRAPPER_CPP_INCLUDED

#define ANS_HI_VER 250	// ANSYS 25.0
#define ANS_LOW_VER 50	// ANSYS 5.0
#define BUFLEN 255

#define ANSLIC "ane3fl"

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
}

void ansysWrapper::run()
{
    this->run(m_apdl);
}

void ansysWrapper::run(QString apdl)
{
    QString fileName;

    if (m_isBatch)
        fileName = m_projectPath + "/" + QString::fromLatin1(INPUTFILE);
	else
    {
       QString ansVersionStr;
       ansVersionStr = QString::number(m_ansVersion);
       //fileName = m_projectPath + "/start" + ansVersionStr + ".ans";
       fileName = m_projectPath + "/start.ans";
    }

    qDebug() <<"temp dir path" << m_projectPath;
    if (!tempDir.isValid())
    {
       qDebug() <<"Temp dir error: " << tempDir.errorString();
    }

    QFile f(fileName);
    if(!f.open(QFile::WriteOnly|QFile::Text))
    {
       qDebug() << "Could not open the file for writing:" << fileName;
       return;
    }
    f.write(apdl.toLatin1());
    f.close();

    qDebug() << "Ansys path and arguments: " << m_arg;
    QProcess pr;
    pr.setWorkingDirectory(m_projectPath);
    pr.start(m_pathToAns, m_arg);
    pr.waitForFinished(INT_MAX);
    qDebug() << "Ansys process output:\n" << pr.readAll();
    qDebug() << "Ansys process finished with code: " << pr.exitCode() << " Status: " << exitCodeToText(pr.exitCode());

    QFile ans_output(tempDir.filePath(m_jobName+".err"));
    if(ans_output.open(QFile::ReadOnly|QFile::Text))
    {
       qDebug() << ans_output.readAll();
       ans_output.close();
    }

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


void ansysWrapper::createFEfromArray(int16_t*** voxels, short int numCubes, int numSeeds)
{
    for (int i = 0; i < numSeeds; i++)
       this->createLocalCS();

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
    QVarLengthArray<float> key(3);
    QMap< QVarLengthArray<float>, int> nodes;
    QVector<int> elemets;
    QVector<int> tmp_elemets(20);
    int node_number = 0;
    for (int i = 0; i < numCubes; i++)
        for (int j = 0; j < numCubes; j++)
            for (int k = 0; k < numCubes; k++)
            {
                for (int l = 1; l <= 20; l++)
                {
                    key[0] = node_coordinates[l][0] + i;
                    key[1] = node_coordinates[l][1] + j;
                    key[2] = node_coordinates[l][2] + k;
                    int current_node = l - 1;
                    if (nodes.contains(key))
                    {
                        tmp_elemets[l-1] = nodes[key];
                    }
                    else
                    {
                        nodes.insert(key, node_number + current_node);
                        tmp_elemets[l-1] = node_number + current_node;
                    }

                }
                node_number += 20;
                elemets.append(tmp_elemets);
            }
    QTextStream apdl(&m_apdl);

    apdl << "NBLOCK,6,SOLID,"/*<< nodes.size() <<", "<<nodes.size()*/ << Qt::endl;
    apdl <<"(3i9,6e21.13e3)" << Qt::endl ;

    for (auto ni = nodes.begin(); ni!=nodes.end(); ni++)
    {
        apdl.setFieldWidth(9);
        apdl << ni.value()+1 << 0 << 0;
        apdl.setFieldWidth(21);
        apdl << QString::number(ni.key()[0], 'E', 13)
             << QString::number(ni.key()[1], 'E', 13)
             << QString::number(ni.key()[2], 'E', 13);
        apdl.setFieldWidth(0);
        apdl << Qt::endl;
    }
    apdl <<"N,R5.3,LOC,-1," << Qt::endl;

    apdl << "EBLOCK,19,SOLID" << Qt::endl;
    apdl << "(19i10)" << Qt::endl;
    for (int ei = 0; ei < elemets.size(); ei+=20)
    {
        key = nodes.key(elemets[ei]);
        qDebug()<< key[0] <<" "<< key[1] <<" "<< key[2];
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

        qDebug()<<"  Coord_sys_id:" << coord_sys_id;
        apdl.setFieldWidth(10);
        apdl << mat_id << el_id << real_const << sec_id << coord_sys_id << bd_flag << sld_ref
             << el_shape << el_num_nodes << exclude_key << el_number;
        for (int j = ei; j < ei+20; j++)
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
    }
    apdl << -1 << Qt::endl; // indicate end of the list
    apdl << Qt::endl;
    apdl << "CSYS, 0"<<Qt::endl; //switch to global coord system
}

int ansysWrapper::createLocalCS()
{
    QTextStream apdl(&m_apdl);
    int cs_id = this->m_lcs;
    double eu_angles[3];
    this->generate_random_angles(eu_angles, true);
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
        qDebug() << "Ansys not found";
        return;
    }
	assert(m_ansVersion != -1);
    env += "\\bin\\";
	m_pathToAns = env;

    env = qEnvironmentVariable("ANSYS_SYSDIR");

	m_pathToAns += env;
    m_pathToAns += "\\ansys" + QString::number(m_ansVersion) + ".exe";
}

int ansysWrapper::kp(double x, double y)
{
    m_apdl += QString::asprintf("k, %d, %g, %g\n", m_kpid, x, y);
    return m_kpid++;
}

int ansysWrapper::spline(std::vector<int> kps, int left_boundary, int right_boundary)
{
    if (kps.size() < 2) return -1;

    m_apdl += QString::asprintf("FLST,3,%lld,3\n", kps.size());

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

void ansysWrapper::generate_random_angles(double *angl, bool in_deg, double epsilon)
{
    if (!angl)
        return;

    std::random_device myRandomDevice;
    auto seed = myRandomDevice();

    std::uniform_real_distribution<double> unif(-1.0, 1.0);
    std::default_random_engine re(seed);

    double g[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            g[i][j] = unif(re);

    double n_phi = acos(g[2][2]);
    double n_phi1, n_phi2;

    if ( (fabs(n_phi) < epsilon) ||  (fabs(n_phi - M_PI) < epsilon) )
    {
        n_phi1 = atan2(g[0][1], g[0][0]);
        n_phi2 = 0;
    }
    else
    {
        n_phi1 = atan2(g[2][0],-g[2][1]);
        n_phi2 = atan2(g[0][2], g[1][2]);
    }

    angl[0] = n_phi1;
    angl[1] = n_phi;
    angl[2] = n_phi2;
    if (in_deg)
    {
        angl[0] = angl[0] / M_PI * 180.0;
        angl[1] = angl[1] / M_PI * 180.0;
        angl[2] = angl[2] / M_PI * 180.0;
    }
}

void ansysWrapper::saveAll()
{
    auto s = R"(

alls     !this selects everything, good place to start
NSEL,S,S,EQV,,, ,0 !- Select nodes with results
!
set,first
*GET,numb_sets,ACTIVE,0,SET,NSET !get number of results sets
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

*cfopen,ls_%current_time%,CSV

*vwrite,'X','Y','Z','UX','UY','UZ','SX','SY','SZ','SXY','SYZ','SXZ','EpsX','EpsY','EpsZ','EpsXY','EpsYZ','EpsXZ'
%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C;%C

*vwrite,nodal_data_comp(1,1),nodal_data_comp(1,2),nodal_data_comp(1,3),nodal_data_comp(1,4), nodal_data_comp(1,5), nodal_data_comp(1,6), nodal_data_comp(1,7), nodal_data_comp(1,8), nodal_data_comp(1,9), nodal_data_comp(1,10), nodal_data_comp(1,11), nodal_data_comp(1,12), nodal_data_comp(1,13),  nodal_data_comp(1,14), nodal_data_comp(1,15), nodal_data_comp(1,16), nodal_data_comp(1,17), nodal_data_comp(1,18)
(E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8";"E16.8)
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

#endif // ANSYSWRAPPER_CPP_INCLUDED

