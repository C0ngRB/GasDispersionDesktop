#pragma once

class ITerrain {
public:
    virtual ~ITerrain() = default;
    virtual float height(double x, double y) const = 0;
    virtual bool isValid() const { return true; }
};
