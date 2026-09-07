#include "speciesview.h"
#include "displayformat.h"

#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <functional>

namespace {

QString joinChecked(const QSet<QString>& keys,
                    const QStringList& order,
                    const std::function<QString(const QString&)>& labelFor)
{
    QStringList labels;
    for (const QString& key : order) {
        if (keys.contains(key))
            labels.append(labelFor(key));
    }
    return labels.isEmpty() ? QStringLiteral("未填写")
                            : labels.join(QStringLiteral("、"));
}

} // namespace

SpeciesViewForm::SpeciesViewForm(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    m_stack->addWidget(buildEmptyPage());
    m_stack->addWidget(buildDetailPage());
    showNode(0);
}

void SpeciesViewForm::setDocument(TaxonomyDocument* document)
{
    m_document = document;
    showNode(m_nodeId);
}

void SpeciesViewForm::setDataDir(const QString& path)
{
    m_dataDir = path;
}

void SpeciesViewForm::showNode(int id)
{
    m_nodeId = id;
    const TaxonNode* n = m_document ? m_document->node(id) : nullptr;
    if (!n) {
        m_hintLabel->setText(QStringLiteral("在左侧选择植物。\n\n"
                                             "点击左侧的“种 / 亚种”节点，"
                                             "这里会显示它的图片和全部属性。"));
        m_stack->setCurrentIndex(0);
        return;
    }

    if (!TaxonRanks::canHostPlantInfo(n->rank)) {
        m_hintLabel->setText(QStringLiteral("当前节点：%1（%2）\n\n"
                                             "这一级只做分类，没有植物属性。\n"
                                             "请点击真正的“种 / 亚种”植物查看。")
                                 .arg(n->name, TaxonRanks::displayName(n->rank)));
        m_stack->setCurrentIndex(0);
        return;
    }

    m_pathLabel->setText(QStringLiteral("%1（%2）")
                             .arg(m_document->displayPathOf(id),
                                  TaxonRanks::displayName(n->rank)));
    m_stack->setCurrentIndex(1);
    if (m_detailScroll)
        m_detailScroll->verticalScrollBar()->setValue(0);

    refreshDetail();
    refreshPhotoList(n->info.photos);
}

void SpeciesViewForm::setEditEnabled(bool enabled)
{
    if (m_editButton)
        m_editButton->setEnabled(enabled);
}

QWidget* SpeciesViewForm::buildEmptyPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    m_hintLabel = new QLabel(page);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("font-size: 15px; color: #555;"));
    layout->addStretch();
    layout->addWidget(m_hintLabel);
    layout->addStretch();
    return page;
}

QLabel* SpeciesViewForm::makeValueLabel(QWidget* parent) const
{
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QWidget* SpeciesViewForm::buildDetailPage()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_detailScroll = scroll;

    auto* content = new QWidget;
    auto* outer = new QVBoxLayout(content);

    m_pathLabel = new QLabel(content);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    outer->addWidget(m_pathLabel);

    m_editButton = new QPushButton(QStringLiteral("编辑资料…"), content);
    m_editButton->setMinimumHeight(30);
    m_editButton->setToolTip(QStringLiteral("在独立窗口中修改这个植物的属性"));
    outer->addWidget(m_editButton, 0, Qt::AlignRight);
    connect(m_editButton, &QPushButton::clicked, this, [this] {
        if (m_nodeId > 0)
            emit editRequested(m_nodeId);
    });

    // ---------------- 图片 ----------------
    auto* photoBox = new QGroupBox(QStringLiteral("植物照片（点击缩略图切换大图）"),
                                   content);
    auto* photoLayout = new QVBoxLayout(photoBox);
    m_photoPreview = new QLabel(photoBox);
    m_photoPreview->setObjectName(QStringLiteral("photoPreview"));
    m_photoPreview->setAlignment(Qt::AlignCenter);
    m_photoPreview->setMinimumHeight(220);
    m_photoPreview->setStyleSheet(
        QStringLiteral("QLabel#photoPreview {"
                       "  border: 1px solid #b0b0b0;"
                       "  background: #f8f8f8;"
                       "  color: #777777;"
                       "  font-size: 14px; }"));
    m_photoPreview->setText(QStringLiteral("暂无照片\n\n可点击“编辑资料…”为这个植物配图。"));
    photoLayout->addWidget(m_photoPreview);

    m_photoList = new QListWidget(photoBox);
    m_photoList->setIconSize(QSize(88, 88));
    m_photoList->setViewMode(QListView::IconMode);
    m_photoList->setResizeMode(QListView::Adjust);
    m_photoList->setMovement(QListView::Static);
    m_photoList->setMinimumHeight(104);
    photoLayout->addWidget(m_photoList);
    outer->addWidget(photoBox);
    connect(m_photoList, &QListWidget::currentItemChanged,
            this, &SpeciesViewForm::onPhotoSelectionChanged);

    // ---------------- 基本信息 ----------------
    auto* identityBox = new QGroupBox(QStringLiteral("基本信息"), content);
    auto* identityForm = new QFormLayout(identityBox);
    m_nameValue = makeValueLabel(identityBox);
    identityForm->addRow(QStringLiteral("中文名："), m_nameValue);
    m_rankValue = makeValueLabel(identityBox);
    identityForm->addRow(QStringLiteral("等级："), m_rankValue);
    m_latinValue = makeValueLabel(identityBox);
    identityForm->addRow(QStringLiteral("拉丁学名："), m_latinValue);
    m_aliasValue = makeValueLabel(identityBox);
    identityForm->addRow(QStringLiteral("别名 / 俗名："), m_aliasValue);
    m_descriptionValue = makeValueLabel(identityBox);
    m_descriptionValue->setMinimumHeight(60);
    identityForm->addRow(QStringLiteral("描述："), m_descriptionValue);
    outer->addWidget(identityBox);

    // ---------------- 环境需求 ----------------
    auto* envBox = new QGroupBox(QStringLiteral("环境需求"), content);
    auto* envForm = new QFormLayout(envBox);
    m_lightValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("光照："), m_lightValue);
    m_waterValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("水分："), m_waterValue);
    m_temperatureValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("温度范围："), m_temperatureValue);
    m_humidityValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("空气湿度："), m_humidityValue);
    m_phValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("土壤 pH："), m_phValue);
    m_soilValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("土壤类型："), m_soilValue);
    m_zoneValue = makeValueLabel(envBox);
    envForm->addRow(QStringLiteral("耐寒区 (USDA)："), m_zoneValue);
    outer->addWidget(envBox);

    // ---------------- 生长形态 ----------------
    auto* growthBox = new QGroupBox(QStringLiteral("生长形态"), content);
    auto* growthForm = new QFormLayout(growthBox);
    m_habitValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("生长习性："), m_habitValue);
    m_lifecycleValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("生命周期："), m_lifecycleValue);
    m_foliageValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("叶型："), m_foliageValue);
    m_growthRateValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("生长速度："), m_growthRateValue);
    m_heightValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("株高："), m_heightValue);
    m_spreadValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("冠幅："), m_spreadValue);
    m_propagationValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("繁殖方式："), m_propagationValue);
    m_usageValue = makeValueLabel(growthBox);
    growthForm->addRow(QStringLiteral("观赏 / 用途："), m_usageValue);
    outer->addWidget(growthBox);

    // ---------------- 物候 ----------------
    auto* phenologyBox = new QGroupBox(QStringLiteral("物候"), content);
    auto* phenologyForm = new QFormLayout(phenologyBox);
    m_bloomValue = makeValueLabel(phenologyBox);
    phenologyForm->addRow(QStringLiteral("花期："), m_bloomValue);
    m_fruitValue = makeValueLabel(phenologyBox);
    phenologyForm->addRow(QStringLiteral("果期："), m_fruitValue);
    outer->addWidget(phenologyBox);

    // ---------------- 扩展属性 ----------------
    auto* customBox = new QGroupBox(QStringLiteral("扩展属性"), content);
    auto* customLayout = new QVBoxLayout(customBox);
    m_customTable = new QTableWidget(0, 2, customBox);
    m_customTable->setHorizontalHeaderLabels(
        { QStringLiteral("属性名"), QStringLiteral("属性值") });
    m_customTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_customTable->verticalHeader()->setVisible(false);
    m_customTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_customTable->setSelectionMode(QAbstractItemView::NoSelection);
    customLayout->addWidget(m_customTable);
    outer->addWidget(customBox);

    scroll->setWidget(content);
    return scroll;
}

void SpeciesViewForm::refreshDetail()
{
    const TaxonNode* n = m_document ? m_document->node(m_nodeId) : nullptr;
    if (!n)
        return;

    const SpeciesInfo empty;
    const SpeciesInfo& info = n->hasInfo ? n->info : empty;

    m_nameValue->setText(n->name);
    m_rankValue->setText(TaxonRanks::displayName(n->rank));
    m_latinValue->setText(info.scientificName.isEmpty()
                              ? QStringLiteral("未填写")
                              : info.scientificName);
    m_aliasValue->setText(info.aliases.isEmpty()
                              ? QStringLiteral("未填写")
                              : info.aliases.join(QStringLiteral("、")));
    m_descriptionValue->setText(info.description.trimmed().isEmpty()
                                    ? QStringLiteral("未填写")
                                    : info.description);

    m_lightValue->setText(lightLabel(info.light));
    m_waterValue->setText(waterLabel(info.water));
    m_temperatureValue->setText(
        DisplayFormat::rangeText(info.temperatureMinC, info.temperatureMaxC,
                                 QStringLiteral(" ℃"), -100));
    m_humidityValue->setText(
        DisplayFormat::rangeText(info.humidityMinPct, info.humidityMaxPct,
                                 QStringLiteral(" %")));
    // 行标签已含“土壤 pH：”，值无需再加单位。
    m_phValue->setText(
        DisplayFormat::rangeTextDouble(info.phMin, info.phMax, QString()));
    m_soilValue->setText(joinChecked(
        info.soilTypes, predefinedSoilTypes(),
        [](const QString& key) { return soilTypeLabel(key); }));
    if (info.hardinessZoneLow == 0 && info.hardinessZoneHigh == 0) {
        m_zoneValue->setText(QStringLiteral("未填写"));
    } else if (info.hardinessZoneLow == 0) {
        m_zoneValue->setText(
            QStringLiteral("最高区 %1").arg(info.hardinessZoneHigh));
    } else if (info.hardinessZoneHigh == 0) {
        m_zoneValue->setText(
            QStringLiteral("最低区 %1").arg(info.hardinessZoneLow));
    } else {
        m_zoneValue->setText(
            QStringLiteral("区 %1 ~ %2")
                .arg(info.hardinessZoneLow)
                .arg(info.hardinessZoneHigh));
    }

    m_habitValue->setText(habitLabel(info.habit));
    m_lifecycleValue->setText(lifecycleLabel(info.lifeCycle));
    m_foliageValue->setText(foliageLabel(info.foliage));
    m_growthRateValue->setText(growthRateLabel(info.growthRate));

    m_heightValue->setText(
        DisplayFormat::rangeText(info.heightMinCm, info.heightMaxCm,
                                 QStringLiteral(" cm")));
    m_spreadValue->setText(
        DisplayFormat::rangeText(info.spreadMinCm, info.spreadMaxCm,
                                 QStringLiteral(" cm")));
    m_propagationValue->setText(joinChecked(
        info.propagationMethods, predefinedPropagationMethods(),
        [](const QString& key) { return propagationLabel(key); }));
    m_usageValue->setText(joinChecked(
        info.usageTags, predefinedUsageTags(),
        [](const QString& key) { return usageTagLabel(key); }));

    m_bloomValue->setText(DisplayFormat::joinMonths(info.bloomMonths));
    m_fruitValue->setText(DisplayFormat::joinMonths(info.fruitMonths));

    m_customTable->setRowCount(0);
    QStringList customKeys = info.custom.keys();
    customKeys.sort();
    for (const QString& key : customKeys) {
        const int row = m_customTable->rowCount();
        m_customTable->insertRow(row);
        m_customTable->setItem(row, 0, new QTableWidgetItem(key));
        m_customTable->setItem(row, 1,
                               new QTableWidgetItem(info.custom.value(key)));
    }
}

QString SpeciesViewForm::resolvePhotoPath(const QString& relativeName) const
{
    if (!isSafePhotoFileName(relativeName))
        return QString();
    return QDir(m_dataDir).filePath(
        QStringLiteral("photos") + QLatin1Char('/') + relativeName);
}

void SpeciesViewForm::refreshPhotoList(const QStringList& photos)
{
    m_photoList->clear();
    QString firstPhoto;
    for (const QString& relativeName : photos) {
        const QString fullPath = resolvePhotoPath(relativeName);
        auto* item = new QListWidgetItem(QFileInfo(relativeName).fileName());
        item->setData(Qt::UserRole, relativeName);
        item->setToolTip(fullPath);
        QPixmap pixmap(fullPath);
        if (!pixmap.isNull()) {
            item->setIcon(QIcon(pixmap.scaled(
                QSize(88, 88), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        } else {
            item->setText(relativeName + QStringLiteral("（缺失）"));
        }
        if (firstPhoto.isEmpty())
            firstPhoto = relativeName;
        m_photoList->addItem(item);
    }

    if (m_photoList->count() > 0) {
        const QSignalBlocker blocker(m_photoList);
        m_photoList->setCurrentRow(0);
    }
    setPhotoPreview(firstPhoto);
}

void SpeciesViewForm::onPhotoSelectionChanged(QListWidgetItem* current,
                                              QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    setPhotoPreview(current ? current->data(Qt::UserRole).toString()
                            : QString());
}

void SpeciesViewForm::setPhotoPreview(const QString& relativeName)
{
    if (relativeName.isEmpty()) {
        m_photoPreviewOriginal = QPixmap();
    } else {
        const QPixmap loaded(resolvePhotoPath(relativeName));
        m_photoPreviewOriginal = loaded.isNull() ? QPixmap() : loaded;
    }
    updatePhotoPreview();
}

void SpeciesViewForm::updatePhotoPreview()
{
    if (!m_photoPreview)
        return;

    if (m_photoPreviewOriginal.isNull()) {
        m_photoPreview->setText(QStringLiteral("暂无照片\n\n"
                                               "可点击“编辑资料…”为这个植物配图。"));
        m_photoPreview->setPixmap(QPixmap());
        return;
    }

    QSize area = m_photoPreview->contentsRect().size();
    if (area.width() <= 0 || area.height() <= 0)
        area = m_photoPreview->size();
    m_photoPreview->setText(QString());
    m_photoPreview->setPixmap(m_photoPreviewOriginal.scaled(
        area, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void SpeciesViewForm::resizeEvent(QResizeEvent* event)
{
    updatePhotoPreview();
    QWidget::resizeEvent(event);
}
