#include "test_datasetbrowser.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <vector>
#include "datasetbrowsercontroller.h"
#include "hdf5wrapper.h"

namespace {

QVariantList itemsOf(const DatasetBrowserController& c, const QString& title)
{
    for (const QVariant& s : c.sections())
        if (s.toMap().value(QStringLiteral("title")).toString() == title)
            return s.toMap().value(QStringLiteral("items")).toList();
    return {};
}

QString valueOf(const DatasetBrowserController& c, const QString& title, const QString& key)
{
    for (const QVariant& i : itemsOf(c, title))
        if (i.toMap().value(QStringLiteral("key")).toString() == key)
            return i.toMap().value(QStringLiteral("value")).toString();
    return QStringLiteral("<missing>");
}

} // namespace

void TestDatasetBrowser::testManifestFiltersAndSections()
{
    // A dataset folder as make_dataset.py writes it: manifest.csv + cells/<id>.hdf5.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir d(dir.path());
    QVERIFY(d.mkpath(QStringLiteral("cells")));
    {
        QFile m(d.filePath(QStringLiteral("manifest.csv")));
        QVERIFY(m.open(QIODevice::WriteOnly | QIODevice::Text));
        m.write("id,group,status,seed,law_id,law_E_GPa,law_nu,law_aniso,p_cells_per_edge,p_lattice_cell_mm,"
                "p_lattice_family,p_relative_density,p_assembly_struts,grid,wall_vox,seconds,exit_code\n");
        m.write("000001,assemblies,done,100000,sampled,2.0,0.38,1.0,1,12.5,cubic_assembly,0.35,3,64,8.1,42.0,0\n");
        m.write("000002,assemblies,unprintable,100001,sampled,2.0,0.38,1.0,1,9.0,cubic_assembly,0.25,2,,,0.3,3\n");
        m.write("000003,plates,deferred,100002,sampled,2.2,0.4,0.8,1,10.0,plate_sc,0.3,,128,4.0,0.5,6\n");
    }
    {
        // The cell of row 1: compliance with known moduli, the parameters JSON and
        // the solver diagnostics, written the way hdf5wrapper.cpp writes them.
        HDF5Wrapper h(d.filePath(QStringLiteral("cells/000001.hdf5")).toStdString());
        h.write("/", "last_set", 1);
        h.write("/1", "cubeSize", 64);
        std::vector<std::vector<float>> S(6, std::vector<float>(6, 0.0f));
        S[0][0] = 1.0f / 200e6f; S[1][1] = 1.0f / 250e6f; S[2][2] = 1.0f / 300e6f;
        S[0][1] = S[1][0] = -0.3f / 200e6f;
        for (int i = 3; i < 6; ++i) S[static_cast<size_t>(i)][static_cast<size_t>(i)] = 0.5f / 50e6f;   // G = 50 MPa
        h.write("/1", "S_matrix", S);
        h.write("/1/geometry", "parameters_json", QStringLiteral(
            R"({"parameters": {"lattice_family_key": "cubic_assembly", "cells_per_edge": 1, "lattice_cell_mm": 12.5,)"
            R"( "relative_density": 0.35, "lattice_achieved_density": 0.351, "lattice_wall_mm": 1.2, "lattice_wall_voxels": 6.1,)"
            R"( "voxel_mm": 0.195, "assembly_struts": 3, "assembly_plates": 2, "assembly_shells": 1, "assembly_holes": 1,)"
            R"( "assembly_spread": 0.3, "mat_e_gpa": 2.0, "mat_nu": 0.38, "mat_aniso": 1.0, "print_verdict": "printable",)"
            R"( "print_components": 1, "print_islands": 0, "print_largest_island_vox": 0, "print_min_wall_mm": 0.8,)"
            R"( "print_thin_fraction": 0.01, "print_unsupported_best": 0.02, "print_best_direction": "+Z",)"
            R"( "print_closed_pore_fraction": 0.0}})"));
        h.write("/1/geometry", "sensitivity_index", 3.5);
        h.write("/1/geometry", "solid_energy_share_loads", std::vector<float>{ 0.99f, 0.98f, 0.995f, 0.97f, 0.99f, 0.99f });
    }

    DatasetBrowserController c;
    QStringList opened;
    c.setCellOpener([&opened](const QString& p) { opened << p; return true; });
    QVERIFY(c.openPath(dir.path()));
    QCOMPARE(c.totalRows(), 3);
    QCOMPARE(c.groups(), (QStringList{ QStringLiteral("All groups"), QStringLiteral("assemblies"), QStringLiteral("plates") }));
    QCOMPARE(c.shownRows(), 1);                               // default filter: rows with a cell file
    QCOMPARE(c.currentId(), QStringLiteral("000001"));        // the first one is opened
    QCOMPARE(opened.size(), 1);
    QVERIFY(opened.front().endsWith(QStringLiteral("cells/000001.hdf5")));

    QCOMPARE(valueOf(c, "Run", "Solve grid"), QStringLiteral("64³"));
    QCOMPARE(c.sections().front().toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Properties"));
    QCOMPARE(valueOf(c, "Geometry", "Struts / plates / shells / windows"), QStringLiteral("3 / 2 / 1 / 1"));
    QCOMPARE(valueOf(c, "Geometry", "Wall"), QStringLiteral("1.2 mm  (6.1 vox)"));
    QCOMPARE(valueOf(c, "Material", "E (same in all directions)"), QStringLiteral("2 GPa"));
    QCOMPARE(c.sections().at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Material"));
    QCOMPARE(valueOf(c, "Printability", "Verdict"), QStringLiteral("printable"));
    QCOMPARE(valueOf(c, "Printability", "Unsupported, best direction"), QStringLiteral("2.00 % along +Z"));
    QCOMPARE(valueOf(c, "Properties", "E x / y / z, MPa"), QStringLiteral("200 / 250 / 300"));
    QCOMPARE(valueOf(c, "Properties", "E max / E min"), QStringLiteral("1.5  (z / x)"));
    QCOMPARE(valueOf(c, "Properties", "G xy / yz / xz, MPa"), QStringLiteral("50 / 50 / 50"));
    QVERIFY(valueOf(c, "Properties", "ν xy / xz / yz").startsWith(QStringLiteral("0.3 / ")));
    QCOMPARE(valueOf(c, "Properties", "Mean E / E solid"), QStringLiteral("0.125"));
    QCOMPARE(valueOf(c, "Properties", "Sensitivity index"), QStringLiteral("3.5"));
    QCOMPARE(valueOf(c, "Properties", "Solid energy share"), QStringLiteral("0.97"));

    // Filters keep the selection when they can, and rows without a file say why.
    c.setStatusFilter(QStringLiteral("All rows"));
    QCOMPARE(c.shownRows(), 3);
    QCOMPARE(c.currentIndex(), 0);
    c.setGroupFilter(QStringLiteral("plates"));
    QCOMPARE(c.shownRows(), 1);
    c.select(0);
    QVERIFY(c.currentFile().isEmpty());
    QVERIFY(c.message().contains(QStringLiteral("deferred")));
    QCOMPARE(valueOf(c, "Geometry", "Cell size"), QStringLiteral("10.0 mm"));   // from the manifest row
    QCOMPARE(opened.size(), 1);                                                  // nothing to open
    c.setGroupFilter(QStringLiteral("All groups"));
    c.setStatusFilter(QStringLiteral("Rejected"));
    QCOMPARE(c.shownRows(), 1);
    c.select(0);
    QVERIFY(c.message().contains(QStringLiteral("rejected")));
    c.setStatusFilter(QStringLiteral("All rows"));
    c.setSearch(QStringLiteral("0003"));
    QCOMPARE(c.shownRows(), 1);

    // A cell file inside cells/ opens its whole dataset with that cell selected.
    DatasetBrowserController c2;
    c2.setCellOpener([](const QString&) { return true; });
    QVERIFY(c2.openPath(d.filePath(QStringLiteral("cells/000001.hdf5"))));
    QCOMPARE(c2.totalRows(), 3);
    QCOMPARE(c2.currentId(), QStringLiteral("000001"));
    c2.next();
    QCOMPARE(c2.currentId(), QStringLiteral("000002"));
    c2.prev();
    QCOMPARE(c2.currentId(), QStringLiteral("000001"));

    // Anything else is refused with a message.
    QVERIFY(!c2.openPath(d.filePath(QStringLiteral("manifest.txt"))));
}
