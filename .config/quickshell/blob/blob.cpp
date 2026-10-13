#include "blob.hpp"

#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
float signedBoxDistance(float px, float py, float cx, float cy, float hx, float hy) {
    const float dx = std::abs(px - cx) - hx;
    const float dy = std::abs(py - cy) - hy;
    return std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) + std::max(dy, 0.0f) * std::max(dy, 0.0f)) + std::min(std::max(dx, dy), 0.0f);
}
float cornerJoinFactor(float distance, float smoothing) {
    const auto smoothstep = [](float edge0, float edge1, float value) {
        const float amount = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return amount * amount * (3.0f - 2.0f * amount);
    };
    return std::max(smoothstep(0.0f, smoothing, distance), smoothstep(0.0f, -smoothing, distance));
}
}

class BlobMaterial final : public QSGMaterial {
public:
    struct Box { float cx, cy, hx, hy, radii[4]; };
    QSGMaterialType* type() const override { static QSGMaterialType type; return &type; }
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode) const override;
    int compare(const QSGMaterial*) const override { return 0; }
    float outer[4] = {}, inner[4] = {}, radius = 0, smoothing = 20, eigenvalue = 1, innerRadii[4] = {};
    QColor color, outlineColor;
    float outlineWidth = 0;
    QList<Box> boxes;
};
class BlobShader final : public QSGMaterialShader {
public:
    BlobShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/blob.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/blob.frag.qsb"));
    }
    bool updateUniformData(RenderState& state, QSGMaterial* next, QSGMaterial*) override {
        auto* material = static_cast<BlobMaterial*>(next);
        QByteArray* data = state.uniformData();
        if (state.isMatrixDirty()) memcpy(data->data(), state.combinedMatrix().constData(), 64);
        if (state.isOpacityDirty()) { const float opacity = state.opacity(); memcpy(data->data() + 64, &opacity, 4); }
        memcpy(data->data() + 68, material->outer, 16); memcpy(data->data() + 84, material->inner, 16);
        memcpy(data->data() + 100, &material->radius, 4); memcpy(data->data() + 104, &material->smoothing, 4); memcpy(data->data() + 108, &material->eigenvalue, 4);
        const int count = qMin(material->boxes.size(), 16); memcpy(data->data() + 112, &count, 4);
        memcpy(data->data() + 128, material->innerRadii, 16);
        const float rgba[] = {float(material->color.redF()), float(material->color.greenF()), float(material->color.blueF()), float(material->color.alphaF())}; memcpy(data->data() + 144, rgba, 16);
        const float outline[] = {float(material->outlineColor.redF()), float(material->outlineColor.greenF()), float(material->outlineColor.blueF()), material->outlineWidth}; memcpy(data->data() + 160, outline, 16);
        for (int i = 0; i < count; ++i) { memcpy(data->data() + 176 + i * 16, &material->boxes[i], 16); memcpy(data->data() + 432 + i * 16, material->boxes[i].radii, 16); }
        return true;
    }
};
QSGMaterialShader* BlobMaterial::createShader(QSGRendererInterface::RenderMode) const { return new BlobShader; }
BlobGroup::BlobGroup(QObject* parent) : QObject(parent) {}
QColor BlobGroup::color() const { return m_color; } qreal BlobGroup::smoothing() const { return m_smoothing; } qreal BlobGroup::eigenvalue() const { return m_eigenvalue; } QColor BlobGroup::outlineColor() const { return m_outlineColor; } qreal BlobGroup::outlineWidth() const { return m_outlineWidth; }
void BlobGroup::setColor(const QColor& value) { if (m_color == value) return; m_color = value; emit colorChanged(); dirty(); } void BlobGroup::setSmoothing(qreal value) { if (qFuzzyCompare(m_smoothing, value)) return; m_smoothing = value; emit smoothingChanged(); dirty(); } void BlobGroup::setEigenvalue(qreal value) { value = qMax<qreal>(0.001, value); if (qFuzzyCompare(m_eigenvalue, value)) return; m_eigenvalue = value; emit eigenvalueChanged(); dirty(); } void BlobGroup::setOutlineColor(const QColor& value) { if (m_outlineColor == value) return; m_outlineColor = value; emit outlineChanged(); dirty(); } void BlobGroup::setOutlineWidth(qreal value) { value = qMax<qreal>(0, value); if (qFuzzyCompare(m_outlineWidth, value)) return; m_outlineWidth = value; emit outlineChanged(); dirty(); }
void BlobGroup::add(Blob* blob) { if (!blobs.contains(blob)) blobs.append(blob); if (blob->frame()) frame = blob; dirty(); } void BlobGroup::remove(Blob* blob) { blobs.removeAll(blob); if (frame == blob) frame = nullptr; dirty(); } void BlobGroup::dirty() { if (frame) frame->update(); }
Blob::Blob(QQuickItem* parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); } Blob::~Blob() { if (m_group) m_group->remove(this); }
void Blob::setGroup(BlobGroup* value) { if (m_group == value) return; if (m_group) m_group->remove(this); m_group = value; if (m_group) m_group->add(this); emit groupChanged(); }
void Blob::setRadius(qreal value) { if (qFuzzyCompare(m_radius, value)) return; m_radius = value; emit radiusChanged(); if (m_group) m_group->dirty(); } void Blob::setTopLeftRadius(qreal value) { if (qFuzzyCompare(m_topLeftRadius, value)) return; m_topLeftRadius = value; emit cornerRadiiChanged(); if (m_group) m_group->dirty(); } void Blob::setTopRightRadius(qreal value) { if (qFuzzyCompare(m_topRightRadius, value)) return; m_topRightRadius = value; emit cornerRadiiChanged(); if (m_group) m_group->dirty(); } void Blob::setBottomRightRadius(qreal value) { if (qFuzzyCompare(m_bottomRightRadius, value)) return; m_bottomRightRadius = value; emit cornerRadiiChanged(); if (m_group) m_group->dirty(); } void Blob::setBottomLeftRadius(qreal value) { if (qFuzzyCompare(m_bottomLeftRadius, value)) return; m_bottomLeftRadius = value; emit cornerRadiiChanged(); if (m_group) m_group->dirty(); }
void Blob::setFrame(bool value) { if (m_frame == value) return; m_frame = value; emit frameChanged(); if (m_group) { if (value) m_group->frame = this; m_group->dirty(); } } void Blob::setBorderLeft(qreal value) { if (qFuzzyCompare(m_left, value)) return; m_left = value; emit borderChanged(); if (m_group) m_group->dirty(); } void Blob::setBorderRight(qreal value) { if (qFuzzyCompare(m_right, value)) return; m_right = value; emit borderChanged(); if (m_group) m_group->dirty(); } void Blob::setBorderTop(qreal value) { if (qFuzzyCompare(m_top, value)) return; m_top = value; emit borderChanged(); if (m_group) m_group->dirty(); } void Blob::setBorderBottom(qreal value) { if (qFuzzyCompare(m_bottom, value)) return; m_bottom = value; emit borderChanged(); if (m_group) m_group->dirty(); }
void Blob::geometryChange(const QRectF& now, const QRectF& old) { QQuickItem::geometryChange(now, old); if (m_group && now != old) m_group->dirty(); }
QSGNode* Blob::updatePaintNode(QSGNode* old, UpdatePaintNodeData*) {
    if (!m_frame || !m_group) { delete old; return nullptr; }
    auto* node = static_cast<QSGGeometryNode*>(old);
    if (!node) { node = new QSGGeometryNode; auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4); geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip); node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry); node->setMaterial(new BlobMaterial); node->setFlag(QSGNode::OwnsMaterial); }
    auto* vertices = node->geometry()->vertexDataAsTexturedPoint2D(); vertices[0].set(0, 0, 0, 0); vertices[1].set(width(), 0, 1, 0); vertices[2].set(0, height(), 0, 1); vertices[3].set(width(), height(), 1, 1); node->geometry()->markVertexDataDirty();
    auto* material = static_cast<BlobMaterial*>(node->material()); material->outer[0] = width() / 2; material->outer[1] = height() / 2; material->outer[2] = width() / 2; material->outer[3] = height() / 2; material->inner[0] = (m_left + width() - m_right) / 2; material->inner[1] = (m_top + height() - m_bottom) / 2; material->inner[2] = qMax<qreal>(0, (width() - m_left - m_right) / 2); material->inner[3] = qMax<qreal>(0, (height() - m_top - m_bottom) / 2); material->radius = m_radius; material->smoothing = m_group->smoothing(); material->eigenvalue = m_group->eigenvalue(); material->color = m_group->color(); material->outlineColor = m_group->outlineColor(); material->outlineWidth = m_group->outlineWidth(); material->innerRadii[0] = topLeftRadius(); material->innerRadii[1] = topRightRadius(); material->innerRadii[2] = bottomRightRadius(); material->innerRadii[3] = bottomLeftRadius(); material->boxes.clear();
    for (Blob* blob : m_group->blobs) if (blob != this && !blob->frame() && blob->width() > 0 && blob->height() > 0) { const QPointF point = blob->mapToItem(this, QPointF(0, 0)); material->boxes.append({float(point.x() + blob->width() / 2), float(point.y() + blob->height() / 2), float(blob->width() / 2), float(blob->height() / 2), {float(blob->topLeftRadius()), float(blob->topRightRadius()), float(blob->bottomRightRadius()), float(blob->bottomLeftRadius())}}); }
    for (auto& box : material->boxes) { float factors[4] = {1, 1, 1, 1}; const float corners[4][2] = {{box.cx - box.hx, box.cy - box.hy}, {box.cx + box.hx, box.cy - box.hy}, {box.cx + box.hx, box.cy + box.hy}, {box.cx - box.hx, box.cy + box.hy}}; for (const auto& neighbor : material->boxes) { if (&neighbor == &box) continue; for (int corner = 0; corner < 4; ++corner) factors[corner] = std::min(factors[corner], cornerJoinFactor(signedBoxDistance(corners[corner][0], corners[corner][1], neighbor.cx, neighbor.cy, neighbor.hx, neighbor.hy), material->smoothing)); } for (int corner = 0; corner < 4; ++corner) box.radii[corner] = std::max(box.radii[corner] * factors[corner], std::min(box.radii[corner], 2.0f)); }
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial); return node;
}
