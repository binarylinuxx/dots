#pragma once

#include <QColor>
#include <QList>
#include <QQuickItem>
#include <qqml.h>

class Blob;

class BlobGroup : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
    Q_PROPERTY(qreal eigenvalue READ eigenvalue WRITE setEigenvalue NOTIFY eigenvalueChanged)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineChanged)
    Q_PROPERTY(qreal outlineWidth READ outlineWidth WRITE setOutlineWidth NOTIFY outlineChanged)
public:
    explicit BlobGroup(QObject* parent = nullptr);
    QColor color() const;
    qreal smoothing() const;
    qreal eigenvalue() const;
    QColor outlineColor() const;
    qreal outlineWidth() const;
    void setColor(const QColor& color);
    void setSmoothing(qreal smoothing);
    void setEigenvalue(qreal eigenvalue);
    void setOutlineColor(const QColor& color);
    void setOutlineWidth(qreal width);
    void add(Blob* blob);
    void remove(Blob* blob);
    void dirty();
    QList<Blob*> blobs;
    Blob* frame = nullptr;
signals:
    void colorChanged();
    void smoothingChanged();
    void eigenvalueChanged();
    void outlineChanged();
private:
    QColor m_color{0x10, 0x14, 0x19};
    qreal m_smoothing = 20;
    qreal m_eigenvalue = 1;
    QColor m_outlineColor{0, 0, 0, 0};
    qreal m_outlineWidth = 0;
};

class Blob : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(BlobGroup* group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(qreal topLeftRadius READ topLeftRadius WRITE setTopLeftRadius NOTIFY cornerRadiiChanged)
    Q_PROPERTY(qreal topRightRadius READ topRightRadius WRITE setTopRightRadius NOTIFY cornerRadiiChanged)
    Q_PROPERTY(qreal bottomRightRadius READ bottomRightRadius WRITE setBottomRightRadius NOTIFY cornerRadiiChanged)
    Q_PROPERTY(qreal bottomLeftRadius READ bottomLeftRadius WRITE setBottomLeftRadius NOTIFY cornerRadiiChanged)
    Q_PROPERTY(bool frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(qreal borderLeft READ borderLeft WRITE setBorderLeft NOTIFY borderChanged)
    Q_PROPERTY(qreal borderRight READ borderRight WRITE setBorderRight NOTIFY borderChanged)
    Q_PROPERTY(qreal borderTop READ borderTop WRITE setBorderTop NOTIFY borderChanged)
    Q_PROPERTY(qreal borderBottom READ borderBottom WRITE setBorderBottom NOTIFY borderChanged)
public:
    explicit Blob(QQuickItem* parent = nullptr);
    ~Blob() override;
    BlobGroup* group() const { return m_group; }
    qreal radius() const { return m_radius; }
    qreal topLeftRadius() const { return m_topLeftRadius < 0 ? m_radius : m_topLeftRadius; }
    qreal topRightRadius() const { return m_topRightRadius < 0 ? m_radius : m_topRightRadius; }
    qreal bottomRightRadius() const { return m_bottomRightRadius < 0 ? m_radius : m_bottomRightRadius; }
    qreal bottomLeftRadius() const { return m_bottomLeftRadius < 0 ? m_radius : m_bottomLeftRadius; }
    bool frame() const { return m_frame; }
    qreal borderLeft() const { return m_left; }
    qreal borderRight() const { return m_right; }
    qreal borderTop() const { return m_top; }
    qreal borderBottom() const { return m_bottom; }
    void setGroup(BlobGroup* group);
    void setRadius(qreal value);
    void setTopLeftRadius(qreal value);
    void setTopRightRadius(qreal value);
    void setBottomRightRadius(qreal value);
    void setBottomLeftRadius(qreal value);
    void setFrame(bool value);
    void setBorderLeft(qreal value);
    void setBorderRight(qreal value);
    void setBorderTop(qreal value);
    void setBorderBottom(qreal value);
signals:
    void groupChanged();
    void radiusChanged();
    void cornerRadiiChanged();
    void frameChanged();
    void borderChanged();
protected:
    void geometryChange(const QRectF& now, const QRectF& old) override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
private:
    BlobGroup* m_group = nullptr;
    qreal m_radius = 32;
    qreal m_topLeftRadius = -1, m_topRightRadius = -1, m_bottomRightRadius = -1, m_bottomLeftRadius = -1;
    qreal m_left = 12, m_right = 12, m_top = 12, m_bottom = 12;
    bool m_frame = false;
};
