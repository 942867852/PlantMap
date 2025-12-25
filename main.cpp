// main.cpp
#include <QApplication>
#include <QDebug>
#include <QDate>
#include "mainwindow.h"
#include "plantmanager.h"
#include "traitregistry.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    PlantManager mgr;

    Plant rose("玫瑰", "美丽芳香", "/images/roses.jpg");
    auto& p1 = rose.properties();
    p1.bloomStart = QDate(2024, 5, 1);
    p1.bloomEnd = QDate(2024, 7, 31);
    p1.lightRange = {PlantProperties::Medium, PlantProperties::High};
    p1.temperatureRange = {15, 30};
    p1.traits.insert(Traits().findTrait("喜阳", TraitCategory::Light));
    p1.traits.insert(Traits().findTrait("开花", TraitCategory::Bloom));

    Plant fern("蕨类", "阴湿环境生长", ":/images/fern.png"); // 资源路径也支持
    auto& p2 = fern.properties();
    p2.lightRange = {PlantProperties::Low, PlantProperties::Medium};
    p2.temperatureRange = {10, 25};
    p2.traits.insert(Traits().findTrait("耐阴", TraitCategory::Light));
    p2.traits.insert(Traits().findTrait("喜湿", TraitCategory::Water));



    // mgr.addPlant(rose);
    // mgr.addPlant(fern);

    // // 🔽 保存到 JSON
    // if (mgr.saveToFile("plants.json")) {
    //     qDebug() << "✅ 数据已保存到 plants.json";
    // }

    // // 清空并重新加载

    // mgr.clear();
    // if (mgr.loadFromFile("plants.json")) {
    //     qDebug() << "\n✅ 成功从文件加载以下植物：";
    //     for (const auto& p : mgr.getAllPlants()) {
    //         qDebug() << "- 名称:" << p.getName()
    //             << "| 图片:" << p.getImagePath()
    //             << "| 花期:" << (p.properties().hasBloomPeriod() ?
    //                                  p.properties().bloomStart.toString() + " ~ " + p.properties().bloomEnd.toString() :
    //                                  "无");
    //     }
    // }

    // MainWindow w;
    // w.show();
    // return a.exec();


}
